#include "train_planner.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "trackdata/track_data.h"
#include "trackdata/track_node_queue.h"

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

void train_state_init(struct TrainState *state) {
  state->direction = TRAINDIR_FORWARD;
}

#define DIRECTIONS_NUM 3
static int ALL_DIRECTIONS[DIRECTIONS_NUM] = {DIR_AHEAD, DIR_STRAIGHT, DIR_CURVED};

void train_planner_task() {
  struct TrackNode track_nodes[TRACK_MAX];
  // trackb_init(track_nodes);
  trackb_init(track_nodes);

  struct TrainState state;
  train_state_init(&state);

  // struct TrackNode *dest;
  struct TrackNode *dest = &track_nodes[72];

  int path_dist = 0;

  // struct TrainPosition pos;
  struct TrainPosition pos = {.dist = 12, .node = &track_nodes[0]};

  struct TrackNodeQueue queue;
  track_node_queue_init(&queue, track_nodes);

  // start from train position
  track_node_queue_add(&queue, pos.node);

  // find path to dest
  int visited[TRACK_MAX] = {0};
  struct TrackNode *node;

  int prev_node[TRACK_MAX];
  int prev_direction[TRACK_MAX];

  visited[pos.node->index] = 1;
  struct TrackNode *track_node;

  while (!track_node_queue_empty(&queue)) {
    track_node = track_node_queue_poll(&queue);

    if (track_node->index == dest->index) {
      // found destination
      break;
    }

    // printf("node %s ",track_node->name);
    if (track_node->type == NODE_BRANCH) {
      node = track_node->edge[DIR_CURVED].dest;
      if (visited[node->index] == 0) {
        // printf("curved %s ",node->name);
        track_node_queue_add(&queue, node);
        prev_node[node->index] = track_node->index;
        prev_direction[node->index] = DIR_CURVED;
        visited[node->index] = 1;
      }
      node = track_node->edge[DIR_STRAIGHT].dest;
      if (visited[node->index] == 0) {
        // printf("straight %s ",node->name);
        track_node_queue_add(&queue, node);
        prev_node[node->index] = track_node->index;
        prev_direction[node->index] = DIR_STRAIGHT;
        visited[node->index] = 1;
      }
    } else if (track_node->type == NODE_MERGE || track_node->type == NODE_SENSOR) {
      node = track_node->edge[DIR_AHEAD].dest;
      if (visited[node->index] == 0) {
        // printf("ahead %s ",node->name);// correct
        track_node_queue_add(&queue, node);
        prev_node[node->index] = track_node->index;
        visited[node->index] = 1;
        prev_direction[node->index] = DIR_AHEAD;
      }
    } else if (track_node->type == NODE_EXIT) {
      continue;
    }
  }
  for (int i = 0;; i++) {
    // print current node & which direction the previous node chose
    printf("%s %d ", track_node->name, prev_direction[track_node->index]);
    if (i % 8 == 0) {
      printf("\r\n");
    }
    if (track_node->index == pos.node->index) {
      break;
    }
    track_node = &track_nodes[prev_direction[track_node->index]];
  }
}
