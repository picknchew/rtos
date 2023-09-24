#include "task.h"

#define MAX_TASKS 32

static unsigned int next_task_id = 0;
static struct TaskDescriptor tasks[MAX_TASKS];

struct TaskDescriptor *current_task;

struct TaskDescriptor *task_get_current_task() {
    return current_task;
}

/**
 * Create a new task and add it to the queue.
 */
int Create(int priority, void (*function)()) {
    // stack grows down
    
}

