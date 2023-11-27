#pragma once

#include "track_path.h"
#include "trackdata/track_data.h"

struct TrackPosition {
  struct TrackNode *node;
  int offset;
};

struct TrainPosition {
  struct TrackPosition position;
  // we need to store this because sometimes we are partically on a branch and we've taken the
  // curved route and reached our destination but the shortest path takes the straight route and
  // doesn't find a valid path.
  int last_dir;
};

struct TrackPosition track_position_random();
struct TrainPosition train_position_add(struct TrainPosition pos, struct Path *path, int offset);
struct TrainPosition train_position_reverse_node(struct TrainPosition pos, int train_tid);
