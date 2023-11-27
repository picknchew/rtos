#include "distance.h"

#include <stdbool.h>

#include "../../server/name_server.h"
#include "../trainset.h"
#include "../trainset_task.h"
#include "track_data.h"

// TESTED
struct TrackDistance
track_distance(struct TrackNode *track, struct TrackNode begin, struct TrackNode end) {
  // struct TrackNode *track;
  // tracka_init(track);
  // from sensor A3
  // struct TrackNode begin = track[2];
  // struct TrackNode sensor = begin;
  struct TrackDistance ret = {.distance = -1, .begin = begin.name, .end = NULL};
  struct TrackNode node = begin;
  int train = WhoIs("train");
  int dist = 0;

  while (true) {
    // nextnode = *sensor.edge[DIR_AHEAD].dest;
    // dist+=sensor.edge[DIR_AHEAD].dist;
    if ((dist != 0) && (node.index == end.index)) {
      // loop
      ret.distance = dist;
      ret.end = node.name;
      return ret;
    }

    if (node.type == NODE_BRANCH) {
      // printf("%s ",node.name);
      enum SwitchDirection direction = TrainGetSwitchState(train, node.num);
      if (direction == DIRECTION_CURVED) {
        dist += node.edge[DIR_CURVED].dist;
        node = *node.edge[DIR_CURVED].dest;
      } else if (direction == DIRECTION_STRAIGHT) {
        dist += node.edge[DIR_STRAIGHT].dist;
        node = *node.edge[DIR_STRAIGHT].dest;
      } else {
        return ret;
      }
    } else if (node.type == NODE_MERGE || node.type == NODE_SENSOR) {
      // printf("%s ",node.name);
      dist += node.edge[DIR_AHEAD].dist;
      node = *node.edge[DIR_AHEAD].dest;
    } else if (node.type == NODE_EXIT) {
      // reach the exit of the track
      ret.distance = dist;
      ret.end = node.name;
      return ret;
    } else {
      return ret;
    }
  }
}
