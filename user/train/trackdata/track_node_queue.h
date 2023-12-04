#include <stdbool.h>

#include "track_data.h"

struct TrackNodeQueueNode {
  struct TrackNode *val;
  struct TrackNodeQueueNode *next;
  struct TrackNodeQueueNode *prev;
};

struct TrackNodeQueue {
  struct TrackNodeQueueNode nodes[TRACK_MAX];
  struct TrackNodeQueueNode *head;
  struct TrackNodeQueueNode *tail;
  unsigned int size;
};

void track_node_queue_init(struct TrackNodeQueue *queue, struct TrackNode *track_nodes);

void track_node_queue_add(struct TrackNodeQueue *queue, struct TrackNode *track);

struct TrackNode *track_node_queue_poll(struct TrackNodeQueue *queue);
struct TrackNode *track_node_queue_pop(struct TrackNodeQueue *queue);
struct TrackNode *track_node_queue_head(struct TrackNodeQueue *queue);
struct TrackNode *track_node_queue_peek_tail(struct TrackNodeQueue *queue);

bool track_node_queue_empty(struct TrackNodeQueue *queue);
