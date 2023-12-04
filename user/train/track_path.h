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
  struct TrackPosition dest;
  bool reverse;
};
