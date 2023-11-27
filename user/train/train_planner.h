#pragma once

#include <stdbool.h>

#include "track_position.h"
#include "trackdata/track_data.h"
#include "track_path.h"

struct RoutePlan {
  struct Path path;
  struct SimplePath paths[TRACK_EDGE_MAX];
  struct TrackPosition src;
  struct TrackPosition dest;
  unsigned int paths_len;
  bool path_found;
};

void train_planner_task();

struct RoutePlan CreatePlan(int tid, struct TrainPosition *src, struct TrackPosition *dest);
void TrackCrossed(int tid, struct TrackEdge *edge);
