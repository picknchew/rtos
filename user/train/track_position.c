#include "track_position.h"

#include "selected_track.h"
#include "trackdata/track_data.h"
#include "train_planner.h"
#include "trainset_task.h"
#include "util.h"

struct TrackPosition track_position_random() {
  struct TrackNode *node = &track[rand() % TRACK_MAX];
  // TODO: generate random number for offset?
  struct TrackPosition pos = {.node = node, .offset = 0};
  return pos;
}

// TODO: augment Path node directions on top of graph to figure out positioning after adding and
// subtracting

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

// returns a position relative to the same direction of pos
struct TrackPosition track_position_subtract(struct TrackPosition pos, int train_tid, int offset) {
  int new_offset = pos.offset - offset;
  if (new_offset >= 0) {
    struct TrackPosition new_pos = {.node = pos.node, .offset = new_offset};
    return new_pos;
  }

  // we need to go backwards if new_offset is negative
  // we turn the offset into a positive offset in the opposite direction
  new_offset = offset - pos.offset;

  struct TrackNode *cur_node = pos.node->reverse;
  enum SwitchDirection next_dir = cur_node->type == NODE_BRANCH
                                      ? TrainGetSwitchState(train_tid, cur_node->num)
                                      : TRAINSET_DIRECTION_STRAIGHT;
  struct TrackEdge *next_edge =
      &cur_node->edge[next_dir == TRAINSET_DIRECTION_STRAIGHT ? DIR_AHEAD : DIR_CURVED];

  while (new_offset >= next_edge->dist) {
    new_offset -= next_edge->dist;
    cur_node = next_edge->dest;
    next_dir = cur_node->type == NODE_BRANCH ? TrainGetSwitchState(train_tid, cur_node->num)
                                             : TRAINSET_DIRECTION_STRAIGHT;
    next_edge = &cur_node->edge[next_dir == TRAINSET_DIRECTION_STRAIGHT ? DIR_AHEAD : DIR_CURVED];
  }

  // reverse offset direction
  new_offset = next_edge->dist - new_offset;
  struct TrackNode *node = next_edge->dest->reverse;

  struct TrackPosition new_pos = {.node = node, .offset = new_offset};
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
