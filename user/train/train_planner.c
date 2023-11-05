#include "train_planner.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

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

    node->val = &track_nodes[i];
    node->next = NULL;
  }
}

void track_node_queue_add(struct TrackNodeQueue *queue, struct TrackNode *track) {
  struct TrackNodeQueueNode *node = &queue->nodes[track->index];
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

struct TrackNode * track_node_queue_head(struct TrackNodeQueue *queue) {
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
  // trackb_init(track_nodes);
  trackb_init(track_nodes);


  struct TrainState state;
  train_state_init(&state);


  // struct TrackNode *dest;
  struct TrackNode *dest = &track_nodes[72];


  int path_dist = 0;


  // struct TrainPosition pos;
  struct TrainPosition pos = {.dist = 12,.node = &track_nodes[0]};


  struct TrackNodeQueue queue;
  track_node_queue_init(&queue, track_nodes);


  // start from train position
  track_node_queue_add(&queue, pos.node);


  // find path to dest
  int visited[TRACK_MAX] =  {0};
  struct TrackNode *node;
  int pred[TRACK_MAX];
  int direction[TRACK_MAX];

  visited[pos.node->index] = 1;
  struct TrackNode *track_node;


  while (!track_node_queue_empty(&queue)) {
    track_node = track_node_queue_poll(&queue);


    if (track_node->index == dest->index) {
      // found destination
      break;
    }

    // printf("node %s ",track_node->name);
    if (track_node->type == NODE_BRANCH){
      node = track_node->edge[DIR_CURVED].dest;
      if (visited[node->index]==0) {
        // printf("curved %s ",node->name);
        track_node_queue_add(&queue, node);
        pred[node->index] = track_node->index;
        direction[node->index] = DIR_CURVED;
        visited[node->index] = 1;
      }
      node = track_node->edge[DIR_STRAIGHT].dest;
      if (visited[node->index]==0) {
        // printf("straight %s ",node->name);
        track_node_queue_add(&queue, node);
        pred[node->index] = track_node->index;
        direction[node->index] = DIR_STRAIGHT;
        visited[node->index] = 1;
      }
    }else if (track_node->type==NODE_MERGE || track_node->type==NODE_SENSOR){
      node = track_node->edge[DIR_AHEAD].dest;
      if (visited[node->index]==0) {
        // printf("ahead %s ",node->name);// correct
        track_node_queue_add(&queue, node);
        pred[node->index] = track_node->index;
        visited[node->index] = 1;
        direction[node->index] = DIR_AHEAD;
      }
    }else if (track_node->type==NODE_EXIT){
      continue;
    }
  }
  for(int i=0;;i++){
    // print current node & which direction the previous node chose
    printf("%s %d ",track_node->name,direction[track_node->index]);
    if (i%8==0) printf("\r\n");
    if (track_node->index==pos.node->index) break;
    track_node = &track_nodes[pred[track_node->index]];
  }

}