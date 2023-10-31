#include "../trackdata/track_data.h"

// direction of train in relation to track graph
struct TrainDirection {
  FORWARD, REVERSE
};

struct TrainPosition {
  // position of train relative to the start of a track from the direction
  // the train is heading from
  unsigned int dist;
  struct TrackNode *node;
}

struct TrainState {
  struct TrainDirection direction;
};

void train_state_init(struct TrainState *state) {
  state->direction;
}

#define DIRECTIONS_NUM 3
static int ALL_DIRECTIONS[DIRECTIONS_NUM] = {DIR_AHEAD, DIR_STRAIGHT, DIR_CURVED};

void train_planner_task() {
  struct TrackNode track_node[TRACK_MAX];
  trackb_init(track_node);

  struct TrainState state;
  train_state_init(&state);

  struct TrackNode dest;

  // find path to dest
  for (int i = 0; DIRECTIONS_NUM; ++i) {
    ALL_DIRECTIONS[i];
  }
}
