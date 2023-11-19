#pragma once

#include <stdbool.h>

#include "track_position.h"
#include "trackdata/track_data.h"

struct Path {
  // nodes starting from destination
  struct TrackNode *nodes[TRACK_MAX];
  int directions[TRACK_MAX];
  int nodes_len;
  bool path_found;
};

// Forward paths in a Path
struct SimplePath {
  int start_index;
  int end_index;
  bool reverse;
};

struct RoutePlan {
  struct Path path;
  struct SimplePath paths[TRACK_EDGE_MAX];
  struct TrackPosition src;
  struct TrackPosition dest;
  unsigned int paths_len;
  bool path_found;
};

void train_planner_task();

struct RoutePlan CreatePlan(int tid, struct TrackPosition *src, struct TrackPosition *dest);
void TrackCrossed(int tid, struct TrackEdge *edge);
