#include "tid_queue.h"

#include <stdbool.h>

#include "../task.h"

void tid_queue_init(struct TIDQueue *queue) {
  queue->head = NULL;
  queue->tail = NULL;
  queue->size = 0;

  for (int i = 0; i < TASKS_MAX; ++i) {
    struct TIDQueueNode *node = &queue->nodes[i];

    node->tid = i;
    node->next = NULL;
  }
}

void tid_queue_add(struct TIDQueue *queue, int tid) {
  struct TIDQueueNode *node = &queue->nodes[tid];

  node->next = NULL;

  if (queue->tail) {
    queue->tail->next = node;
  } else {
    queue->head = node;
  }

  queue->tail = node;
  ++queue->size;
}

int tid_queue_poll(struct TIDQueue *queue) {
  struct TIDQueueNode *popped = queue->head;

  queue->head = popped->next;
  --queue->size;

  if (queue->size <= 1) {
    queue->tail = queue->head;
  }

  return popped->tid;
}

int tid_queue_head(struct TIDQueue *queue) {
  return queue->head->tid;
}

bool tid_queue_empty(struct TIDQueue *queue) {
  return queue->size == 0;
}
