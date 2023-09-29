

#include "rpi.h"
#include "syscall.h"
#include "task.h"
#include "task_queue.h"
#include "exception.h"

static const int SYSCALL_TYPE_MASK = 0xFFFF;

static const int EC_MASK = 0x3F;
// syscall exception class
static const int EC_SVC = 0x15;

static const int CONSOLE = 1;

// return back to user mode
extern void kern_exit();

void handle_exception(uint64_t exception_info) {
  int exception_class = (exception_info >> 26) & EC_MASK;

  if (exception_class != EC_SVC) {
    uart_printf(CONSOLE, "not svc exception! %d\r\n", exception_class);
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
        (uint32_t) current_task->context.registers[0], // priority
        (void (*)()) current_task->context.registers[1] // function
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
    default:
      break;
  }

  // always yield to switch to higher priority task or roundrobin
  syscall_yield();

  if (task_get_current_task() == NULL) {
    // no more tasks to run
    for (;;) {} // spin forever
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
