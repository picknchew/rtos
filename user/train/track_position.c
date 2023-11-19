#include "track_position.h"

#include "selected_track.h"
#include "trackdata/track_data.h";

struct TrackPosition track_position_random() {
  struct TrackNode *node = &track[rand() % TRACK_MAX];
  // TODO: generate random number for offset?
  struct TrackPosition pos = {.node = node, .offset = 0};
  return pos;
}

// // returns a track position with node relative to current position.
// // offset can be positive or negative.
// struct TrackPosition track_position_add(struct TrackPosition pos, struct Path path, int offset) {
//   struct TrackEdge *edge = pos.edge;
//   int new_offset = pos.offset + offset;

//   // we need path to determine the next node that we'll be on.
//   if (new_offset < 0) {
//     edge = edge->reverse;
//     new_offset = -new_offset;
//   }

//   while (node->dist >= new_offset) {
//     new_offset -= node->edge[plan->directions[i]].dist;
//     node = node->edge[plan->directions[i]].dest;
//   }

//   struct TrackPosition new_pos = {.node = node, .offset = new_offset};
//   return new_pos;
// }

// // track position for the reverse node
// // reversed track node track positions are not normalized.
// struct TrackPosition track_position_reverse_node(struct TrackPosition pos) {
//   struct TrackEdge *edge = pos.edge->reverse;
//   // TODO: we need to measure train length to reverse the front of the train or from one sensor
//   // contact to the other and add it to the offset. + len to offset
//   struct offset = edge->dist - pos.offset;

//   while (offset >= edge->dist) {
//     offset -= edge->dist;
//     switch (edge->dest.type) {
//       case NODE_BRANCH:
//         // since we only reverse the node when a train reverses, if a train is
//         // on a branch, it must be according to the current direction of the switch
//         // otherwise the train is in an invalid state or an error has occurred.
//         edge = edge->dest.edge[TrainsetGetSwitchState(edge->dest.num)];
//         break;
//       case NODE_MERGE:
//       case NODE_ENTER:
//       case NODE_SENSOR:
//         edge = edge->dest.edge[DIR_AHEAD];
//         break;
//       case NODE_EXIT:
//       default:
//         break;
//     }
//   }

//   struct TrackPosition new_pos = {.edge = edge, .offset = offset};
//   return new_pos
// }
