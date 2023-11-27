#include "track_node.h"

struct TrackDistance {
  int distance;
  const char *begin;
  const char *end;
};

struct TrackDistance
track_distance(struct TrackNode *track, struct TrackNode begin, struct TrackNode end);
