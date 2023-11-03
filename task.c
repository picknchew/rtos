#include "task.h"

#include "rpi.h"
#include "syscall.h"
#include "task_queue.h"

static struct TaskDescriptor tasks[TASKS_MAX] = {{0}};
struct TaskDescriptor *current_task = NULL;

// tasks ready to run
static struct PriorityTaskQueue ready_queue;

void tasks_init() {
  current_task = NULL;

  for (int i = 0; i < TASKS_MAX; ++i) {
    struct TaskDescriptor *task = &tasks[i];

    task->tid = i;
    task->status = TASK_EXITED;

    mail_queue_init(&task->wait_for_receive);

    task->receive_buffer.tid = NULL;
    task->receive_buffer.msg = NULL;
    task->receive_buffer.msglen = 0;

    mail_init(&task->outgoing_msg, task);

    task->tempnode.next = NULL;
    task->tempnode.val = NULL;
  }

  priority_task_queue_init(&ready_queue);
}

static struct TaskDescriptor *task_get_free_task() {
  // iterate over tasks to get next free task
  for (int i = 0; i < TASKS_MAX; ++i) {
    if (tasks[i].status == TASK_EXITED) {
      return &tasks[i];
    }
  }

  // no more free tasks
  return NULL;
}

struct TaskDescriptor *
task_create(struct TaskDescriptor *parent, int priority, void (*function)()) {
  struct TaskDescriptor *task = task_get_free_task();

  if (task == NULL) {
    // out of free tasks
    return NULL;
  }

  struct TaskContext *context = &task->context;

  task->parent = parent;
  task->priority = priority;
  task->status = TASK_READY;

  for (int i = 0; i < NUM_REGISTERS; ++i) {
    context->registers[i] = i;
  }

  // add to get end of stack since it grows up
  context->sp = (uint64_t) task->stack + STACK_SIZE;
  context->lr = (uint64_t) function;
  context->pstate = 0;

  return task;
}

struct TaskDescriptor *task_get_current_task() {
  return current_task;
}

struct TaskDescriptor *task_get_by_tid(int tid) {
  return &tasks[tid];
}

void task_yield_current_task() {
  // don't put threads that are blocked due to message passing or events into
  // task ready queue, the initial task has a status of ready.
  if (current_task != NULL && current_task->status == TASK_ACTIVE) {
    current_task->status = TASK_READY;
    priority_task_queue_push(&ready_queue, current_task);
  }

  current_task = priority_task_queue_pop(&ready_queue);

  if (current_task != NULL) {
    current_task->status = TASK_ACTIVE;
  }
}

void task_print() {
  for (int i = 0; i < 15; ++i) {
    printf("task status %d, %d\r\n", i, tasks[i].status);
  }
}

void task_schedule(struct TaskDescriptor *task) {
  task->status = TASK_READY;
  priority_task_queue_push(&ready_queue, task);
}

void task_exit_current_task() {
  current_task->wait_for_receive.head = NULL;
  current_task->wait_for_receive.tail = NULL;
  current_task->wait_for_receive.size = 0;

  current_task->receive_buffer.tid = NULL;
  current_task->receive_buffer.msg = NULL;
  current_task->receive_buffer.msglen = 0;

  current_task->outgoing_msg.msg = NULL;
  current_task->outgoing_msg.msglen = 0;
  current_task->outgoing_msg.sender = NULL;

  current_task->reply_msg.msg = NULL;
  current_task->reply_msg.msglen = 0;
  current_task->reply_msg.sender = NULL;  // unused

  current_task->tempnode.next = NULL;
  current_task->tempnode.val = NULL;

  current_task->status = TASK_EXITED;
  current_task = NULL;
}
