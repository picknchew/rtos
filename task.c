#include "rpi.h"
#include "task.h"
#include "syscall.h"

void task1();

static struct TaskDescriptor tasks[TASKS_MAX] = { {0} };
struct TaskDescriptor *current_task = NULL;

void task1() {
  uart_puts(1, "task 1 running....");
  Exit();
  uart_puts(1, "second time running");
  Exit();
  uart_puts(1, "third time running");
  Exit();
  uart_puts(1, "fourth time running");
  Exit();
}

void tasks_init() {
  for (int i = 0; i < TASKS_MAX; ++i) {
    struct TaskDescriptor *task = &tasks[i];

    task->tid = i;
    task->status = TASK_EXITED;
  }

  current_task = task_create(0, task1);
}

static struct TaskDescriptor *task_get_free_task() {
  for (int i = 0; i < TASKS_MAX; ++i) {
    if (tasks[i].status == TASK_EXITED) {
      return &tasks[i];
    }
  }

  // no more free tasks
  return NULL;
}

struct TaskDescriptor *task_create(int priority, void (*function)()) {
  struct TaskDescriptor *task = task_get_free_task();
  struct TaskContext *context = &task->context;

  task->priority = priority;

  for (int i = 0; i < NUM_REGISTERS; ++i) {
    context->registers[i] = 0;
  }

  // add to get end of stack since it grows up
  context->sp = (uint64_t) task->stack + STACK_SIZE;
  context->lr = (uint64_t) function;
  context->pstate = 0;

  return task;
}

// iterate over tasks to get next free task

struct TaskDescriptor *task_get_current_task() {
    return current_task;
}

struct TaskDescriptor *task_get_by_tid(int tid) {
    return &tasks[tid];
}

void task_schedule() {
    
}
