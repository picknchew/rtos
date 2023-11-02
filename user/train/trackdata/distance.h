#include "track_node.h"

struct TrackDistance{
    int distance;
    char *begin;
    char *end;
};

struct TrackDistance track_distance(struct TrackNode *track, struct TrackNode begin);
