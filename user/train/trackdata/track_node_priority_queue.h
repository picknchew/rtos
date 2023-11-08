#include <stdbool.h>

#include "track_data.h"

struct TrackNodePriorityQueueNode {
  int priority;
  struct TrackNode *val;
};

struct TrackNodePriorityQueue {
  struct TrackNodePriorityQueueNode nodes[TRACK_MAX];
  struct TrackNodePriorityQueueNode *arr[TRACK_MAX];
  unsigned int size;
};

void track_node_priority_queue_init(
    struct TrackNodePriorityQueue *queue,
    struct TrackNode *track_nodes
);

void track_node_priority_queue_add(
    struct TrackNodePriorityQueue *queue,
    struct TrackNode *track,
    int priority
);

void track_node_priority_queue_decrease_priority(
    struct TrackNodePriorityQueue *queue,
    struct TrackNode *node,
    int priority
);

struct TrackNodePriorityQueueNode *track_node_priority_queue_poll(struct TrackNodePriorityQueue *queue);
struct TrackNodePriorityQueueNode *track_node_priority_queue_head(struct TrackNodePriorityQueue *queue);

bool track_node_priority_queue_empty(struct TrackNodePriorityQueue *queue);
