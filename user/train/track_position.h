#pragma once

#include "trackdata/track_data.h"
#include "train_planner.h"

struct TrackPosition {
  struct TrackNode *node;
  int offset;
};

struct TrackPosition track_position_random();
struct TrackPosition track_position_add(struct TrackPosition pos, struct Path *path, int offset);
