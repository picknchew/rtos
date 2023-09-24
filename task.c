#include "task.h"

static struct TaskDescriptor tasks[TASKS_MAX] = { {0} };
struct TaskDescriptor *current_task = NULL;

void tasks_init() {
  for (int i = 0; i < TASKS_MAX; ++i) {
    struct TaskDescriptor *task = &tasks[i];

    task->tid = i;
    task->status = TASK_EXITED;
  }
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
