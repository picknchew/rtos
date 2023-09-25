#include <stddef.h>

#include "task.h"

#define MAX_PRIORITY 64

struct TaskQueueNode {
  struct TaskDescriptor *val;
  struct TaskQueueNode *next;
};

// simple linked list
struct TaskQueue {
  struct TaskQueueNode *head;
  struct TaskQueueNode *tail;
  size_t size;
};

struct PriorityTaskQueue {
  // indexed by priority
  // add 1 to include priority 0
  struct TaskQueue queues[MAX_PRIORITY];
};

void task_queues_init();

void priority_task_queue_init(struct PriorityTaskQueue *queue);

struct TaskDescriptor *priority_task_queue_pop(struct PriorityTaskQueue *queue);
void priority_task_queue_push(struct PriorityTaskQueue *queue, struct TaskDescriptor *descriptor);
