#pragma once

#include "track_path.h"
#include "trackdata/track_data.h"

struct TrackPosition {
  struct TrackNode *node;
  int offset;
};

struct TrackPosition track_position_random();
struct TrackPosition track_position_add(struct TrackPosition pos, struct Path *path, int offset);
struct TrackPosition
track_position_subtract(struct TrackPosition pos, int train_tid, int offset);
