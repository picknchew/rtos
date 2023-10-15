#include "irq.h"
#include "task_queue.h"

struct EventBlockedTaskQueue {
  // indexed by priority
  // add 1 to include priority 0
  struct TaskQueue queues[EVENT_MAX];
};

void event_blocked_task_queue_init(struct EventBlockedTaskQueue *queue);

struct TaskDescriptor *event_blocked_task_queue_pop(
    struct EventBlockedTaskQueue *queue,
    enum Event event);

void event_blocked_task_queue_push(
    struct EventBlockedTaskQueue *queue,
    struct TaskDescriptor *task,
    enum Event event);

int event_blocked_task_queue_size(struct EventBlockedTaskQueue *queue, enum Event event);
