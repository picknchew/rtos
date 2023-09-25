#include "exception.h"

#include "rpi.h"
#include "syscall.h"
#include "task.h"

static const int SYSCALL_TYPE_MASK = 0xFFFF;

static const int EC_MASK = 0x3F;
// syscall exception class
static const int EC_SVC = 0x15;

static const int CONSOLE = 1;

// return back to user mode
extern void kern_exit();

void handle_exception(int exception_info) {
  uart_puts(CONSOLE, "handle_exception - start\r\n");

  int exception_class = (exception_info >> 26) & EC_MASK;

  if (exception_class != EC_SVC) {
    uart_printf(CONSOLE, "not svc exception! %d\r\n", exception_class);
  }

  struct TaskDescriptor *current_task = task_get_current_task();
  enum SyscallType syscall_type = exception_info & SYSCALL_TYPE_MASK;

  int result  = 0;
  switch (syscall_type) {
    case SYSCALL_EXIT:
      uart_puts(CONSOLE, "ex_test\r\n");
      break;
    case SYSCALL_CREATE:
    // read parameter from ...
      int priority = current_task->context.registers[0];
      void* function = current_task->context.registers[1];
      result = (current_task,priority, function);
      break;
    case SYSCALL_MY_TID:
      result = syscall_my_tid(current_task);
      break;
    case SYSCALL_MY_PARENT_TID:
      result = syscall_my_parent_tid(current_task);
      break;
    case SYSCALL_YIELD:
      syscall_yield();
      break;
    default:
      break;
  }

  uart_printf(CONSOLE, "syscall type: %d\r\n", syscall_type);
  uart_puts(CONSOLE, "handle_exception - end\r\n");

  // for (;;) {
  //   // spin
  // }
  return result;
}

int syscall_create(struct TaskDescriptor *task, int priority,void (*code)()){
  // create task descriptor 
  struct TaskDescriptor *td = task_create(priority,code);
  priority_task_queue_push(getPriorityQueue(),task);
  task->status = TASK_READY;
  // TODO task.tid is not defined
  return task->tid;
}

int syscall_my_tid(struct TaskDescriptor *task){
  return task->tid;
}

int syscall_my_parent_tid(struct TaskDescriptor *task){
  return  task->parent->tid;
}

void syscall_yield(){
  // do nothing
}

void syscall_exit(struct TaskDescriptor *current){
  priority_task_queue_delete(getPriorityQueue(),current);
  current->status = TASK_EXITED;
  current->tid = current->tid+TASKS_MAX;
  current->parent = NULL;
  current->priority = 0;
  current->stack[0] = 0;

}


