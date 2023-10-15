#include "event_task_queue.h"

#include "irq.h"

void event_blocked_task_queue_init(struct EventBlockedTaskQueue *queue) {
  // init queues
  for (int i = 0; i < EVENT_MAX; ++i) {
    struct TaskQueue *task_queue = &queue->queues[i];
    task_queue_init(task_queue);
  }
}

int event_blocked_task_queue_size(struct EventBlockedTaskQueue *queue, enum Event event) {
  struct TaskQueue *task_queue = task_queue = &queue->queues[event];
  return task_queue_size(task_queue);
}

struct TaskDescriptor *event_blocked_task_queue_pop(
    struct EventBlockedTaskQueue *queue,
    enum Event event) {
  struct TaskQueue *task_queue = task_queue = &queue->queues[event];
  return task_queue_pop(task_queue)->val;
}

void event_blocked_task_queue_push(
    struct EventBlockedTaskQueue *queue,
    struct TaskDescriptor *task,
    enum Event event) {
  task_queue_add(&queue->queues[event], task);
}
