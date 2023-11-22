#include "track_position.h"

#include "selected_track.h"
#include "trackdata/track_data.h"
#include "train_planner.h"
#include "util.h"

struct TrackPosition track_position_random() {
  struct TrackNode *node = &track[rand() % TRACK_MAX];
  // TODO: generate random number for offset?
  struct TrackPosition pos = {.node = node, .offset = 0};
  return pos;
}

// returns a track position with node relative to current position.
struct TrackPosition track_position_add(struct TrackPosition pos, struct Path *path, int offset) {
  struct TrackNode *node = pos.node;
  int new_offset = pos.offset + offset;
  // index of node in path
  int node_index = -1;

  for (int i = path->nodes_len - 1; i >= 0; --i) {
    if (path->nodes[i] == node) {
      node_index = i;
      break;
    }
  }

  if (node_index == -1) {
    // should never happen since we should always be in a path.
    struct TrackPosition new_pos = {.node = node, .offset = new_offset};
    return new_pos;
  }

  // we need path to determine the next node that we'll be on.
  while (node_index >= 0 && node->edge[path->directions[node_index]].dist <= new_offset) {
    int dir = path->directions[node_index];

    new_offset -= node->edge[dir].dist;
    node = node->edge[dir].dest;

    --node_index;
  }

  struct TrackPosition new_pos = {.node = node, .offset = new_offset};
  return new_pos;
}

struct TrackPosition
track_position_subtract(struct TrackPosition pos, struct Path *path, int offset) {
  struct TrackNode *node = pos.node;

  int new_offset = pos.offset - offset;

  if (new_offset >= 0) {
    struct TrackPosition new_pos = {.node = node, .offset = new_offset};
    return new_pos;
  }

  // index of node in path
  int node_index = -1;

  for (int i = path->nodes_len - 1; i >= 0; --i) {
    if (path->nodes[i] == node) {
      node_index = i;
      break;
    }
  }

  if (node_index == -1) {
    // should never happen since we should always be in a path.
    struct TrackPosition new_pos = {.node = node, .offset = new_offset};
    return new_pos;
  }

  while (node_index < path->nodes_len - 1 && new_offset < 0) {
    ++node_index;

    int dir = path->directions[node_index];
    struct TrackEdge *prev_edge = &path->nodes[node_index]->edge[dir];

    new_offset = prev_edge->dist + new_offset;
  }

  struct TrackPosition new_pos = {.node = path->nodes[node_index], .offset = new_offset};
  return new_pos;
}

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
