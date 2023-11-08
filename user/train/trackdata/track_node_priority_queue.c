// implementation ref: https://www.digitalocean.com/community/tutorials/min-heap-binary-tree

#include "track_node_priority_queue.h"

#include <stdbool.h>
#include <stddef.h>

#include "track_data.h"

static int parent(int i) {
  // Get the index of the parent
  return (i - 1) / 2;
}

static int left_child(int i) {
  return (2 * i + 1);
}

static int right_child(int i) {
  return (2 * i + 2);
}

static void heapify(struct TrackNodePriorityQueue *queue, unsigned int index) {
  if (queue->size <= 1) {
    return;
  }

  unsigned int left = left_child(index);
  unsigned int right = right_child(index);

  unsigned int smallest = index;

  if (left < queue->size && queue->arr[left]->priority < queue->arr[index]->priority) {
    smallest = left;
  }

  if (right < queue->size && queue->arr[right]->priority < queue->arr[smallest]->priority) {
    smallest = right;
  }

  if (smallest != index) {
    struct TrackNodePriorityQueueNode *temp = queue->arr[index];

    queue->arr[index] = queue->arr[smallest];
    queue->arr[smallest] = temp;
    heapify(queue, smallest);
  }
}

void track_node_priority_queue_init(
    struct TrackNodePriorityQueue *queue,
    struct TrackNode *track_nodes
) {
  queue->size = 0;

  for (unsigned int i = 0; i < TRACK_MAX; ++i) {
    struct TrackNodePriorityQueueNode *node = &queue->nodes[i];

    node->val = &track_nodes[i];
    node->priority = 0;

    queue->arr[i] = NULL;
  }
}

void track_node_priority_queue_decrease_priority(
    struct TrackNodePriorityQueue *queue,
    struct TrackNode *node,
    int priority
) {
  int index = 0;

  for (unsigned int i = 0; i < queue->size; ++i) {
    if (queue->arr[i]->val == node) {
      index = i;
      break;
    }
  }

  queue->arr[index]->priority = priority;

  // sift up
  int parent_index = parent(index);
  while (index > 0 && queue->arr[parent_index]->priority > queue->arr[index]->priority) {
    struct TrackNodePriorityQueueNode *temp = queue->arr[index];

    queue->arr[index] = queue->arr[parent_index];
    queue->arr[parent_index] = temp;

    index = parent_index;
    parent_index = parent(index);
  }
}

void track_node_priority_queue_add(
    struct TrackNodePriorityQueue *queue,
    struct TrackNode *track,
    int priority
) {
  unsigned int curr = queue->size;

  queue->arr[curr] = &queue->nodes[track->index];
  queue->arr[curr]->priority = priority;

  while (curr > 0 && queue->arr[parent(curr)]->priority > queue->arr[curr]->priority) {
    // swap
    struct TrackNodePriorityQueueNode *temp = queue->arr[parent(curr)];

    queue->arr[parent(curr)] = queue->arr[curr];
    queue->arr[curr] = temp;

    curr = parent(curr);
  }

  ++queue->size;
}

struct TrackNodePriorityQueueNode *track_node_priority_queue_poll(struct TrackNodePriorityQueue *queue) {
  unsigned int size = --queue->size;
  struct TrackNodePriorityQueueNode *ret = queue->arr[0];

  // set head to last element
  queue->arr[0] = queue->arr[size];

  heapify(queue, 0);
  return ret;
}

struct TrackNodePriorityQueueNode *track_node_priority_queue_head(struct TrackNodePriorityQueue *queue) {
  return queue->arr[0];
}

bool track_node_priority_queue_empty(struct TrackNodePriorityQueue *queue) {
  return queue->size == 0;
}
