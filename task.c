#include "task.h"

#define MAX_TASKS 32

static unsigned int next_task_id = 0;
static struct TaskDescriptor tasks[MAX_TASKS];

struct TaskDescriptor *current_task;

struct TaskDescriptor *task_get_current_task() {
    return current_task;
}
