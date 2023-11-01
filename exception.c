#include "exception.h"

#include "irq.h"
#include "rpi.h"
#include "syscall.h"
#include "task.h"
#include "task_queue.h"
#include "util.h"

static const int SYSCALL_TYPE_MASK = 0xFFFF;

static const int EC_MASK = 0x3F;
// syscall exception class
static const int EC_SVC = 0x15;

static const int CONSOLE = 1;

// return back to user mode
extern void kern_exit();

void handle_invalid_exception() {
  printf("handle_invalid_exception: invalid exception!\r\n");
  for (;;) {}
}

void handle_exception(uint64_t exception_info) {
  int exception_class = (exception_info >> 26) & EC_MASK;

  if (exception_class != EC_SVC) {
    printf("handle_exception: not svc exception! %d\r\n", exception_class);
    for (;;) {}
  }

  struct TaskDescriptor *current_task = task_get_current_task();
  enum SyscallType syscall_type = exception_info & SYSCALL_TYPE_MASK;

  switch (syscall_type) {
    case SYSCALL_EXIT:
      syscall_exit(current_task);
      break;
    case SYSCALL_CREATE:
      current_task->context.registers[0] = syscall_create(
          current_task,
          (uint32_t) current_task->context.registers[0],   // priority
          (void (*)()) current_task->context.registers[1]  // function
      );
      break;
    case SYSCALL_MY_TID:
      current_task->context.registers[0] = syscall_my_tid(current_task);
      break;
    case SYSCALL_MY_PARENT_TID:
      current_task->context.registers[0] = syscall_my_parent_tid(current_task);
      break;
    case SYSCALL_YIELD:
      break;
    case SYSCALL_INIT:
      // do not call yield and run first task.
      break;
    case SYSCALL_SEND:
      current_task->context.registers[0] = syscall_send(
          current_task,
          (int) current_task->context.registers[0],           // tid
          (const char *) current_task->context.registers[1],  // msg
          (int) current_task->context.registers[2],           // msglen
          (char *) current_task->context.registers[3],        // reply
          (int) current_task->context.registers[4]            // rplen
      );
      break;
    case SYSCALL_RECEIVE:
      current_task->context.registers[0] = syscall_receive(
          current_task,
          (int *) current_task->context.registers[0],
          (char *) current_task->context.registers[1],
          (int) current_task->context.registers[2]);
      break;
    case SYSCALL_REPLY:
      current_task->context.registers[0] = syscall_reply(
          (int) current_task->context.registers[0],
          (const char *) current_task->context.registers[1],
          (int) current_task->context.registers[2]);
      break;
    case SYSCALL_AWAIT_EVENT:
      current_task->context.registers[0] =
          syscall_await_event((int) current_task->context.registers[0]);
    default:
      break;
  }

  syscall_yield();

  if (task_get_current_task() == NULL) {
    // no more tasks to run
    printf("exception_handler: no current task\r\n");
    for (;;) {}  // spin forever
  }

  // run task
  kern_exit();
}

int syscall_create(struct TaskDescriptor *parent, int priority, void (*code)()) {
  if (priority <= 0 || priority >= MAX_PRIORITY) {
    // invalid priority
    return -1;
  }

  struct TaskDescriptor *td = task_create(parent, priority, code);

  if (td == NULL) {
    return -2;
  }

  task_schedule(td);
  return td->tid;
}

int syscall_my_tid(struct TaskDescriptor *task) {
  return task->tid;
}

int syscall_my_parent_tid(struct TaskDescriptor *task) {
  if (task->parent == NULL) {
    return -1;
  }

  return task->parent->tid;
}

void syscall_yield() {
  task_yield_current_task();
}

void syscall_exit() {
  task_exit_current_task();
}

// send to tid; send info - msg; length of msg - msglen; reply buffer; reply max length
int syscall_send(
    struct TaskDescriptor *sender,
    int tid,
    const char *msg,
    int msglen,
    char *reply,
    int rplen) {
  struct TaskDescriptor *receiver = task_get_by_tid(tid);

  // task is not running
  if (receiver->status == TASK_EXITED) {
    return -1;
  }

  // send-receive-reply transactions could not be completed: -2

  sender->outgoing_msg.sender = sender;
  sender->outgoing_msg.msg = msg;
  sender->outgoing_msg.msglen = msglen;
  sender->reply_msg.msg = reply;
  sender->reply_msg.msglen = rplen;

  sender->tempnode.val = &sender->outgoing_msg;
  sender->tempnode.next = NULL;

  /*
   * Send/Receive Scenario 1: Send first
   * On Ts doing Send(Tr,…), kernel finds that receiver task Tr is not in ReceiveWait state
   * kernel blocks sender (Ready -> SendWait)
   * kernel adds Ts to list of senders blocked waiting for Tr to receive
   */
  if (receiver->status != TASK_SEND_BLOCKED) {
    // move from ready queue do not push
    sender->status = TASK_RECEIVE_BLOCKED;
    sender->blocked = receiver->tid;

    // put mailNode to receiver's wait_for_receive queue
    mail_queue_add(&receiver->wait_for_receive, &sender->tempnode);
  } else {
    /*
     * Send/Receive Scenario 2: Receive first - checked
     * On Ts doing Send(Tr,…), kernel finds that Tr is in ReceiveWait state
     * Ts blocks (Ready->ReplyWait)
     * Add Ts to list of tasks waiting for reply from Tr
     * Tr becomes unblocks (ReceiveWait -> Ready)
     * kernel copies message from Ts to Tr
     */

    // move from ready queue do not push
    sender->status = TASK_REPLY_BLOCKED;

    // msg copy and overflow detection
    *(receiver->receive_buffer.tid) = sender->tid;
    memcpy(
        receiver->receive_buffer.msg,
        sender->outgoing_msg.msg,
        min(receiver->receive_buffer.msglen, sender->outgoing_msg.msglen));

    // set status to READY and push to ready_queue
    task_schedule(receiver);
  }

  return msglen;
}

// tid - who sent; msg - save to my buffer; msglen - max i can recv
int syscall_receive(struct TaskDescriptor *receiver, int *tid, char *msg, int msglen) {
  /*
   * Send/Receive Scenario 2: Receive first - checked
   * On Tr doing Receive(), kernel finds there are no waiting Sends for Tr
   * Tr blocks (Ready -> ReceiveWait)
   */
  if (mail_queue_size(&receiver->wait_for_receive) == 0) {
    receiver->receive_buffer.tid = tid;
    receiver->receive_buffer.msg = msg;
    receiver->receive_buffer.msglen = msglen;
    receiver->status = TASK_SEND_BLOCKED;

    return -1;
  } else {
    /*
     * Send/Receive Scenario 1: Send first
     * On Tr doing Receive()
     * kernel sees that Tr has waiting sender
     * kernel moves Ts from SendWait to ReplyWait
     * kernel copies message from Ts to Tr
     * Tr remains Ready
     */
    struct Message *incoming_msg = mail_queue_pop(&receiver->wait_for_receive)->val;

    struct TaskDescriptor *sender = incoming_msg->sender;
    sender->status = TASK_REPLY_BLOCKED;
    *tid = sender->tid;

    int len = min(msglen, incoming_msg->msglen);
    memcpy(msg, incoming_msg->msg, len);

    return len;
  }
}

/*
 * When Tr eventuallly does Reply(Ts,…)
 * kernel checks that Ts is in ReplyWait state and on list of tasks waiting for reply from Tr
 * Tr remains ready
 * Ts unblocks (ReplyWait -> Ready)
 * kernel copies reply from Tr to Ts
 */
int syscall_reply(int tid, const char *reply, int rplen) {
  // the tid to reply to
  struct TaskDescriptor *sender = task_get_by_tid(tid);
  if (sender->status == TASK_EXITED) {
    return -1;
  }

  if (sender->status != TASK_REPLY_BLOCKED) {
    return -2;
  }

  struct Message *reply_msg = &sender->reply_msg;

  // reply to the sender
  int length = min(reply_msg->msglen, rplen);
  memcpy(reply_msg->msg, reply, length);

  // set status to READY and push to ready queue
  task_schedule(sender);
  return length;
}

int syscall_await_event(int event_id) {
  if (event_id < 0 || event_id > EVENT_MAX) {
    return -1;
  }

  irq_await_event(event_id);
  // this is replaced with the correct data when the task is unblocked by the interrupt
  return 0;
}
