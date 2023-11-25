#include "train_planner.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#include "selected_track.h"
#include "syscall.h"
#include "track_position.h"
#include "trackdata/track_data.h"
#include "trackdata/track_node_priority_queue.h"
#include "trackdata/track_node_queue.h"
#include "user/server/name_server.h"
#include "user/terminal/terminal_task.h"

// cost of reversing in terms of distance.
// ideally should be the distance that can be covered if we do not
// stop then reaccelerate
const int REVERSE_COST = 200;

// we set directions for paths so that all trains
// travel in the same direction if a train already has a path that goes
// through the track to prevent deadlocks. when all calculated paths have been
// traversed, we remove the set direction for the track.
// stores the number of calculated paths that have set its direction.
static int reserved_track_directions[TRACK_EDGE_MAX];

// if the direction for the this TrackEdge is set to the same direction
// of the TrackEdge passed in.
static bool is_track_available(struct TrackEdge *edge) {
  // printf("is track available");
  struct TrackEdge *reverse_edge = edge->reverse;

  // reverse TrackEdge direction reserved
  if (reserved_track_directions[reverse_edge->index] > 0) {
    // printf("%d\r\n", edge);
    // printf("track not available %d at %d", reserved_track_directions[reverse_edge->index],
    // reverse_edge->index);
    return false;
  }

  return true;
}

static void reserve_track_direction(struct TrackEdge *edge) {
  ++reserved_track_directions[edge->index];
}

static struct TrackNodePriorityQueue queue;

// Dijikstra's algorithm
static struct Path get_shortest_path(struct TrackNode *src, struct TrackNode *dest) {
  // cost includes cost for reversing.
  int cost[TRACK_MAX] = {0};
  // initially filled with null
  struct TrackNode *prev[TRACK_MAX] = {0};
  int directions[TRACK_MAX] = {0};

  for (int i = 0; i < TRACK_MAX; ++i) {
    struct TrackNode *node = &track[i];
    if (node != src && node) {
      cost[i] = INT_MAX;
      prev[i] = NULL;
    }

    track_node_priority_queue_add(&queue, node, cost[i]);
  }

  directions[src->index] = DIR_AHEAD;
  // directions[src->reverse->index] = DIR_REVERSE;

  while (!track_node_priority_queue_empty(&queue)) {
    struct TrackNodePriorityQueueNode *queue_node = track_node_priority_queue_poll(&queue);

    // has never been visited, do not go to neighbours or we will overflow.
    if (queue_node->priority == INT_MAX) {
      continue;
    }

    struct TrackNode *node = queue_node->val;
    struct TrackNode *neighbour;
    int alt_cost = 0;

    switch (node->type) {
      case NODE_BRANCH:
        // if a track direction is unavailable, we try to find an alternative path.
        if (is_track_available(&node->edge[DIR_CURVED])) {
          neighbour = node->edge[DIR_CURVED].dest;
          alt_cost = cost[node->index] + node->edge[DIR_CURVED].dist;

          if (alt_cost < cost[neighbour->index]) {
            cost[neighbour->index] = alt_cost;
            prev[neighbour->index] = node;
            directions[neighbour->index] = DIR_CURVED;
            track_node_priority_queue_decrease_priority(&queue, neighbour, alt_cost);
          }
        }

        // DIR_AHEAD and DIR_STRAIGHT are the same.
        // fall through
      case NODE_MERGE:
      case NODE_ENTER:
      case NODE_SENSOR:
        // reverse
        neighbour = node->reverse;
        // cost of reversing
        alt_cost = cost[node->index] + REVERSE_COST;

        if (alt_cost < cost[neighbour->index]) {
          cost[neighbour->index] = alt_cost;
          prev[neighbour->index] = node;
          directions[neighbour->index] = DIR_REVERSE;
          track_node_priority_queue_decrease_priority(&queue, neighbour, alt_cost);
        }

        // forward
        if (!is_track_available(&node->edge[DIR_AHEAD])) {
          break;
        }

        neighbour = node->edge[DIR_AHEAD].dest;
        alt_cost = cost[node->index] + node->edge[DIR_AHEAD].dist;

        if (alt_cost < cost[neighbour->index]) {
          cost[neighbour->index] = alt_cost;
          prev[neighbour->index] = node;
          directions[neighbour->index] = DIR_AHEAD;
          track_node_priority_queue_decrease_priority(&queue, neighbour, alt_cost);
        }
        break;
      case NODE_EXIT:
      default:
        break;
    }
  }

  struct Path path = {.directions = {0}, .nodes_len = 0, .path_found = false};

  bool dest_reversed = false;
  // if sensor in reverse direction is less costly, path to reverse direction.
  if (cost[dest->reverse->index] < cost[dest->index]) {
    dest = dest->reverse;
    dest_reversed = true;
  }

  if (cost[dest->index] != INT_MAX) {
    path.path_found = true;

    // work backwards to get full path
    struct TrackNode *path_node = dest;
    struct TrackNode *prev_node = NULL;

    while (path_node && (path_node != src)) {
      // remove extra reverse at the end.
      if (dest_reversed && path_node == dest->reverse) {
        path_node = prev[path_node->index];
        continue;
      }

      path.nodes[path.nodes_len] = path_node;
      // increment direction by one to associate direction with the branch that follows this
      // track.
      path.directions[++path.nodes_len] = directions[path_node->index];
      prev_node = path_node;
      path_node = prev[path_node->index];
    }

    // add starting node
    if (prev_node && prev_node == path_node->reverse) {
      // set starting node direction to be reverse
      directions[path_node->index] = DIR_REVERSE;
    }

    path.nodes[path.nodes_len] = path_node;
    path.directions[path.nodes_len++] = directions[path_node->index];
  }

  return path;
}

struct RoutePlan
route_plan_init(struct Path *path, struct TrackPosition *src, struct TrackPosition *dest) {
  struct RoutePlan plan = {
      .path = *path, .paths_len = 0, .path_found = path->path_found, .src = *src, .dest = *dest
  };

  if (!path->path_found) {
    return plan;
  }

  int cur_path_start_index = path->nodes_len - 1;

  for (int i = path->nodes_len - 1; i >= 0; --i) {
    if (plan.path.directions[i] == DIR_REVERSE) {
      // case where we reverse at the start
      if (cur_path_start_index != i) {
        plan.paths[plan.paths_len].start_index = cur_path_start_index;
        plan.paths[plan.paths_len++].end_index = i + 1;
      }

      // create path of len 1 with reverse node.
      plan.paths[plan.paths_len].start_index = i;
      plan.paths[plan.paths_len].end_index = i;
      plan.paths[plan.paths_len++].reverse = true;

      cur_path_start_index = i - 1;
    }
  }

  // finish last simple path
  if (cur_path_start_index >= 0) {
    plan.paths[plan.paths_len].start_index = cur_path_start_index;
    plan.paths[plan.paths_len++].end_index = 0;
  }

  return plan;
}

enum TrainPlannerRequestType { CREATE_PLAN, TRACK_CROSSED };

struct TrainPlannerCreatePlanRequest {
  struct TrackPosition *src;
  struct TrackPosition *dest;
};

struct TrainPlannerTrackCrossedRequest {
  struct TrackEdge *edge;
};

struct TrainPlannerRequest {
  enum TrainPlannerRequestType type;

  union {
    struct TrainPlannerCreatePlanRequest create_plan_req;
    struct TrainPlannerTrackCrossedRequest track_crossed_req;
  };
};

static struct RoutePlan handle_create_plan(struct TrainPlannerCreatePlanRequest *req) {
  int terminal = WhoIs("terminal");
  struct Path path = get_shortest_path(req->src->node, req->dest->node);

  if (path.path_found && path.nodes[0] == req->dest->node->reverse) {
    int dest_dir = path.directions[0];
    int reverse_offset = req->dest->node->edge[dest_dir].dist - req->dest->offset;
    // change destination to match path if it goes to the reverse node
    struct TrackPosition route_dest = {.node = path.nodes[0], .offset = reverse_offset};
    struct RoutePlan plan = route_plan_init(&path, req->src, &route_dest);

    return plan;
  }

  struct RoutePlan plan = route_plan_init(&path, req->src, req->dest);
  return plan;
}

static void handle_track_crossed(struct TrainPlannerTrackCrossedRequest *req) {
  struct TrackEdge *edge = req->edge;

  if (reserved_track_directions[edge->index] == 0) {
    return;
  }

  --reserved_track_directions[edge->index];
}

void train_planner_task() {
  RegisterAs("train_planner");

  track_node_priority_queue_init(&queue, track);
  for (int i = 0; i < TRACK_EDGE_MAX; ++i) {
    reserved_track_directions[i] = 0;
  }

  int tid;
  struct TrainPlannerRequest req;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case CREATE_PLAN: {
        struct RoutePlan plan = handle_create_plan(&req.create_plan_req);
        Reply(tid, (const char *) &plan, sizeof(plan));
        break;
      }
      case TRACK_CROSSED:
        handle_track_crossed(&req.track_crossed_req);
        Reply(tid, NULL, 0);
        break;
    }
  }
}

struct RoutePlan CreatePlan(int tid, struct TrackPosition *src, struct TrackPosition *dest) {
  struct TrainPlannerRequest req = {
      .type = CREATE_PLAN, .create_plan_req = {.src = src, .dest = dest}
  };

  struct RoutePlan plan;
  Send(tid, (const char *) &req, sizeof(req), (char *) &plan, sizeof(plan));
  return plan;
}

// used to signal to the planner that a train has crossed over a track
// whose direction was reserved for a plan.
void TrackCrossed(int tid, struct TrackEdge *edge) {
  struct TrainPlannerRequest req = {.type = TRACK_CROSSED, .track_crossed_req = {.edge = edge}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
