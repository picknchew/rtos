#include "task.h"

#include "task_queue.h"
#include "test_tasks.h"
#include "rpi.h"
#include "syscall.h"
#include "debug.h"

void task1();

static struct TaskDescriptor tasks[TASKS_MAX] = { {0} };
struct TaskDescriptor *current_task = NULL;

// tasks ready to run
static struct PriorityTaskQueue ready_queue;

void task2() {
  for (int i = 0; i < 30; ++i) {
    uart_printf(1, "task 2 counter: %d\r\n", i);
    Yield();
  }

  Exit();
}

void task1() {
  // debug_dump_registers();
  // uart_puts(1, "task 1 running....\r\n");

  // uart_puts(1, "after printing:\r\n");
  // debug_dump_registers();
  // debug_dump_context();
  // // debug_dump_registers();
  // // debug_dump_context();

  // Exit();

  // uart_puts(1, "after restoration\r\n");
  // debug_dump_registers();
  // debug_dump_context();
  // uart_puts(1, "second time running\r\n");
  // uart_puts(1, "after printing again\r\n");
  // debug_dump_registers();
  // debug_dump_context();
  // // debug_dump_registers();
  // Exit();
  // uart_puts(1, "third time running\r\n");
  // // debug_dump_registers();
  // Exit();
  // uart_puts(1, "fourth time running\r\n");
  // // debug_dump_registers();

  Create(0, task2);

  for (int i = 0; i < 30; ++i) {
    uart_printf(1, "task 1 counter: %d\r\n", i);
    Yield();
  }

  Exit();
}

void tasks_init() {
  for (int i = 0; i < TASKS_MAX; ++i) {
    struct TaskDescriptor *task = &tasks[i];

    task->tid = i;
    task->status = TASK_EXITED;
  }

  priority_task_queue_init(&ready_queue);

  // replace with init/idle task
  current_task = task_create(NULL, 1, test_task);
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

struct TaskDescriptor *task_create(struct TaskDescriptor *parent, int priority, void (*function)()) {
  struct TaskDescriptor *task = task_get_free_task();
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

  // TODO set LR to EXIT(); or something
  // AND PC TO FUNCTION

  return task;
}

struct TaskDescriptor *task_get_current_task() {
    return current_task;
}

struct TaskDescriptor *task_get_by_tid(int tid) {
    return &tasks[tid];
}

void task_yield_current_task() {
  if (current_task != NULL) {
    priority_task_queue_push(&ready_queue, current_task);
  }

  current_task = priority_task_queue_pop(&ready_queue);
}

void task_schedule(struct TaskDescriptor *task) {
  task->status = TASK_READY;
  priority_task_queue_push(&ready_queue, task);
}

void task_exit_current_task() {
  current_task->status = TASK_EXITED;
  current_task = priority_task_queue_pop(&ready_queue);
}
