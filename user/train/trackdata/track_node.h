#pragma once

enum NodeType {
  NODE_NONE,
  NODE_SENSOR,
  NODE_BRANCH,
  NODE_MERGE,
  NODE_ENTER,
  NODE_EXIT,
};

#define DIR_AHEAD 0
#define DIR_STRAIGHT 0
#define DIR_CURVED 1
#define DIR_REVERSE 2

struct TrackEdge {
  struct TrackEdge *reverse;
  struct TrackNode *src, *dest;
  int dist; /* in millimetres */
  int index;
};

struct TrackNode {
  const char *name;
  enum NodeType type;
  int num;                   /* sensor or switch number */
  struct TrackNode *reverse; /* same location, but opposite direction */
  struct TrackEdge edge[2];
  int index;
};
