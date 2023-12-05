#include "track_node_queue.h"

#include <stddef.h>

void track_node_queue_init(struct TrackNodeQueue *queue, struct TrackNode *track_nodes) {
  queue->head = NULL;
  queue->tail = NULL;
  queue->size = 0;

  for (int i = 0; i < TRACK_MAX; ++i) {
    struct TrackNodeQueueNode *node = &queue->nodes[i];

    node->val = &track_nodes[i];
    node->prev = NULL;
    node->next = NULL;
  }
}

void track_node_queue_add(struct TrackNodeQueue *queue, struct TrackNode *track) {
  struct TrackNodeQueueNode *node = &queue->nodes[track->index];
  node->next = NULL;

  if (queue->tail) {
    queue->tail->next = node;
    node->prev = queue->tail;
  } else {
    queue->head = node;
    node->prev = NULL;
  }

  queue->tail = node;
  ++queue->size;
}

struct TrackNode *track_node_queue_pop(struct TrackNodeQueue *queue) {
  struct TrackNodeQueueNode *popped = queue->tail;
  queue->tail = popped->prev;
  --queue->size;

  if (queue->size <= 1) {
    queue->head = queue->head;
  }

  return popped->val;
}

struct TrackNode *track_node_queue_peek_tail(struct TrackNodeQueue *queue) {
  return queue->tail->val;
}

struct TrackNode *track_node_queue_poll(struct TrackNodeQueue *queue) {
  struct TrackNodeQueueNode *popped = queue->head;

  queue->head = popped->next;
  --queue->size;

  if (queue->size <= 1) {
    queue->tail = queue->head;
  }

  return popped->val;
}

struct TrackNode *track_node_queue_head(struct TrackNodeQueue *queue) {
  return queue->head->val;
}

bool track_node_queue_empty(struct TrackNodeQueue *queue) {
  return queue->size == 0;
}
