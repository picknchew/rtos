#pragma once

struct TrackPosition {
  struct TrackNode *node;
  int offset;
};

struct TrackPosition track_position_random();
