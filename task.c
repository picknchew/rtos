#include "task.h"

#include "task_queue.h"
#include "rpi.h"
#include "syscall.h"
#include "user/init_task.h"

void task1();

static struct TaskDescriptor tasks[TASKS_MAX] = { {0} };
struct TaskDescriptor *current_task = NULL;

// tasks ready to run
static struct PriorityTaskQueue ready_queue;

void tasks_init() {
  for (int i = 0; i < TASKS_MAX; ++i) {
    struct TaskDescriptor *task = &tasks[i];

    task->tid = i;
    task->status = TASK_EXITED;
  }

  priority_task_queue_init(&ready_queue);

  // run initial task with lowest priority so that it never runs if there's another task is in the queue
  current_task = task_create(NULL, 0, init_task);
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
  if (current_task != NULL) {
    current_task->status = TASK_READY;
    priority_task_queue_push(&ready_queue, current_task);
  }

  current_task = priority_task_queue_pop(&ready_queue);

  if (current_task != NULL) {
    current_task->status = TASK_ACTIVE;
  }
}

void task_schedule(struct TaskDescriptor *task) {
  task->status = TASK_READY;
  priority_task_queue_push(&ready_queue, task);
}

void task_exit_current_task() {
  current_task->status = TASK_EXITED;
  current_task = priority_task_queue_pop(&ready_queue);
}
