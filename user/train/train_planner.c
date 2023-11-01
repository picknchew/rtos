#include "train_planner.h"

#include <stdbool.h>
#include <stddef.h>

#include "trackdata/track_data.h"

// direction of train in relation to track graph
enum TrainDirection { TRAINDIR_FORWARD, TRAINDIR_REVERSE };

struct TrainPosition {
  // position of train relative to the start of a track from the direction
  // the train is heading from
  unsigned int dist;
  struct TrackNode *node;
};

struct TrainState {
  enum TrainDirection direction;
  struct TrainPosition position;
};

struct TrackNodeQueueNode {
  struct TrackNode *val;
  struct TrackNodeQueueNode *next;
};

struct TrackNodeQueue {
  struct TrackNodeQueueNode nodes[TRACK_MAX];
  struct TrackNodeQueueNode *head;
  struct TrackNodeQueueNode *tail;
  unsigned int size;
};

void track_node_queue_init(struct TrackNodeQueue *queue, struct TrackNode *track_nodes) {
  queue->head = NULL;
  queue->tail = NULL;
  queue->size = 0;

  for (int i = 0; i < TRACK_MAX; ++i) {
    struct TrackNodeQueueNode *node = &queue->nodes[i];

    node->val = track_nodes[i].num;
    node->next = NULL;
  }
}

void track_node_queue_add(struct TrackNodeQueue *queue, struct TrackNode *track) {
  struct TrackNodeQueueNode *node = &queue->nodes[track->num];

  node->next = NULL;

  if (queue->tail) {
    queue->tail->next = node;
  } else {
    queue->head = node;
  }

  queue->tail = node;
  ++queue->size;
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

int track_node_queue_head(struct TrackNodeQueue *queue) {
  return queue->head->val;
}

bool track_node_queue_empty(struct TrackNodeQueue *queue) {
  return queue->size == 0;
}

void train_state_init(struct TrainState *state) {
  state->direction = TRAINDIR_FORWARD;
}

#define DIRECTIONS_NUM 3
static int ALL_DIRECTIONS[DIRECTIONS_NUM] = {DIR_AHEAD, DIR_STRAIGHT, DIR_CURVED};

void train_planner_task() {
  struct TrackNode track_nodes[TRACK_MAX];
  trackb_init(track_nodes);

  struct TrainState state;
  train_state_init(&state);

  struct TrackNode *dest;

  int path_dist = 0;

  struct TrainPosition pos;

  struct TrackNodeQueue queue;
  track_node_queue_init(&queue, track_nodes);

  // start from train position
  track_node_queue_add(&queue, pos.node);

  // find path to dest

  while (!track_node_queue_empty(&queue)) {
    struct TrackNode *track_node = track_node_queue_poll(&queue);

    if (track_node == dest) {
      // found destination
      break;
    }

    for (int i = 0; DIRECTIONS_NUM; ++i) {
      ALL_DIRECTIONS[i];

      // dir ahead and straight are the same need to check if track is a turnout.
      
    }
  }
}
