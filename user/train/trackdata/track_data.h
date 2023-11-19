/* THIS FILE IS GENERATED CODE -- DO NOT EDIT */
#pragma once

#include "track_node.h"

// The track initialization functions expect an array of this size.
#define TRACK_MAX 144

// 2 possible edges per TrackNode
#define TRACK_EDGE_MAX TRACK_MAX * 2

void tracka_init(struct TrackNode *track);
void trackb_init(struct TrackNode *track);
