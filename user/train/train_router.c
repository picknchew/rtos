#include "train_router.h"

#include <limits.h>

#include "rpi.h"
#include "syscall.h"
#include "trackdata/track_data.h"
#include "trackdata/track_node_priority_queue.h"
#include "trackdata/track_node_queue.h"
#include "train_sensor_notifier.h"
#include "trainset.h"
#include "trainset_task.h"
#include "user/server/clock_server.h"
#include "user/server/name_server.h"

#define TRAIN_SPEED_MAX 14

// add 1 to include 0
static int TRAINSET_MEASURED_SPEEDS[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
static int TRAINSET_STOPPING_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
static int TRAINSET_STOPPING_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];

static void init_measured_speeds() {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    TRAINSET_MEASURED_SPEEDS[i][0] = 0;
    TRAINSET_STOPPING_DISTANCES[i][0] = 0;
    TRAINSET_STOPPING_TIMES[i][0] = 0;
  }

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(77)][14] = 1;
}

enum TrainRouterRequestType {
  TRAIN_ROUTER_BEGIN_ROUTE,
  TRAIN_ROUTER_UPDATE_SENSORS,
  TRAIN_ROUTER_TICK
};

struct TrainRouterBeginRouteRequest {
  int train;
  int speed;
  char *sensor;
  // in mm
  int sensor_offset;
};

struct TrainRouterUpdateSensorsRequest {
  bool *sensors;
};

struct TrainRouterRequest {
  enum TrainRouterRequestType type;

  union {
    struct TrainRouterUpdateSensorsRequest update_sens_req;
    struct TrainRouterBeginRouteRequest begin_route_req;
  };
};

// time to wait for train to hit constant speed (5s) in ticks
static const int ACCELERATION_DURATION = 500000;

static struct TrackNode track[TRACK_MAX];

static struct TrackNode *track_get_sensor(char *sensor) {
  int index = trainset_get_sensor_index(sensor);
  // first nodes are all sensors
  return &track[index];
}

// direction of train in relation to track graph
enum TrainDirection { TRAINDIR_FORWARD, TRAINDIR_REVERSE, TRAINDIR_UNKNOWN };

struct TrackPosition {
  // position of train relative to the start of a track from the direction
  // the train is heading from
  unsigned int offset;
  struct TrackNode *node;
};

enum PlanState { PLAN_NONE, PLAN_ACCELERATING, PLAN_CALCULATING_PATH, PLAN_WAITING_TO_STOP };

struct Path {
  // nodes starting from destination
  struct TrackNode *nodes[TRACK_MAX];
  int directions[TRACK_MAX];
  int nodes_len;
  int distance;
};

struct Plan {
  enum PlanState state;
  int state_update_time;

  struct Path path;
  struct TrackPosition dest;
  struct TrackPosition stopping_pos;

  int time_to_stop;
  int last_sensor_index;
};

struct TrainState {
  int train;

  enum TrainDirection direction;
  struct TrackPosition last_known_pos;
  int last_known_pos_time;

  struct Plan plan;

  int speed;
};

void trainstate_init(struct TrainState *state, int train) {
  state->direction = TRAINDIR_UNKNOWN;
  state->last_known_pos_time = 0;
  state->speed = 0;
  state->train = train;
}

enum TrainDirection trainstate_get_direction(struct TrainState *state) {
  // get direction train is in from which sensor last triggered
  // we define forward position to be all odd sensors (A1, A3, etc.) (node.num % 2 == 0)
  // sensor numbers are 0 indexed, whereas their names are 1-indexed.
  if (state->last_known_pos.node->num % 2 == 0) {
    return TRAINDIR_FORWARD;
  }

  return TRAINDIR_REVERSE;
}

// offset from last sensor
static int trainstate_get_current_offset(struct TrainState *state, int current_time) {
  // velocity * time since last sensor = offset
  return TRAINSET_MEASURED_SPEEDS[state->train][state->speed] * current_time;
}

static void path_set_switches(struct Path *path, int train_tid) {
  for (int i = path->nodes_len - 1; i >= 0; --i) {
    struct TrackNode *node = path->nodes[i];

    // switch
    if (node->type == NODE_BRANCH) {
      switch (path->directions[i]) {
        case DIR_AHEAD:
          TrainSetSwitchDir(train_tid, node->num, TRAINSET_DIRECTION_STRAIGHT);
          break;
        case DIR_CURVED:
          TrainSetSwitchDir(train_tid, node->num, TRAINSET_DIRECTION_CURVED);
          break;
      }
    }
  }
}

// Dijikstra's algorithm
static struct Path get_shortest_path(struct TrainState *state, struct TrackNode *dest) {
  int dist[TRACK_MAX] = {0};
  // initially filled with null
  struct TrackNode *prev[TRACK_MAX] = {0};
  int directions[TRACK_MAX] = {0};

  struct TrackNodePriorityQueue queue;
  track_node_priority_queue_init(&queue, track);

  struct TrackNode *source = state->last_known_pos.node;

  for (int i = 0; i < TRACK_MAX; ++i) {
    struct TrackNode *node = &track[i];
    if (node != source) {
      dist[i] = INT_MAX;
      prev[i] = NULL;
    }

    track_node_priority_queue_add(&queue, node, dist[i]);
  }

  while (!track_node_priority_queue_empty(&queue)) {
    struct TrackNodePriorityQueueNode *queue_node = track_node_priority_queue_poll(&queue);

    // has never been visited, do not go to neighbours or we will overflow.
    if (queue_node->priority == INT_MAX) {
      continue;
    }

    struct TrackNode *node = queue_node->val;
    struct TrackNode *neighbour;
    int alt = 0;

    switch (node->type) {
      case NODE_BRANCH:
        // printf("curved %s ",node->name);
        neighbour = node->edge[DIR_CURVED].dest;
        alt = dist[node->index] + node->edge[DIR_CURVED].dist;

        if (alt < dist[neighbour->index]) {
          dist[neighbour->index] = alt;
          prev[neighbour->index] = node;
          directions[neighbour->index] = DIR_CURVED;
          track_node_priority_queue_decrease_priority(&queue, neighbour, alt);
        }

        neighbour = node->edge[DIR_STRAIGHT].dest;
        alt = dist[node->index] + node->edge[DIR_STRAIGHT].dist;

        if (alt < dist[neighbour->index]) {
          dist[neighbour->index] = alt;
          prev[neighbour->index] = node;
          directions[neighbour->index] = DIR_STRAIGHT;
          track_node_priority_queue_decrease_priority(&queue, neighbour, alt);
        }
        break;
      case NODE_MERGE:
      case NODE_SENSOR:
        neighbour = node->edge[DIR_AHEAD].dest;
        alt = dist[node->index] + node->edge[DIR_AHEAD].dist;

        if (alt < dist[neighbour->index]) {
          dist[neighbour->index] = alt;
          prev[neighbour->index] = node;
          directions[neighbour->index] = DIR_AHEAD;
          track_node_priority_queue_decrease_priority(&queue, neighbour, alt);
        }
        break;
      case NODE_EXIT:
      default:
        break;
    }
  }

  struct Path path = {.distance = dist[dest->index], .directions = {0}, .nodes_len = 0};
  // work backwards to get full path
  struct TrackNode *path_node = dest;
  while (path_node && path_node != state->last_known_pos.node) {
    path.nodes[path.nodes_len] = path_node;
    // increment direction by one to associate direction with the branch that follows this
    // track.
    path.directions[++path.nodes_len] = directions[path_node->index];
    path_node = prev[path_node->index];
  }

  // add starting node
  path.nodes[path.nodes_len] = state->last_known_pos.node;
  path.directions[path.nodes_len++] = DIR_AHEAD;

  return path;
}

static int get_next_sensor(struct Plan *plan, struct Path *path) {
  for (int i = plan->last_sensor_index - 1; i >= 0; --i) {
    struct TrackNode *node = path->nodes[i];

    if (node->type == NODE_SENSOR) {
      return i;
    }
  }

  return -1;
}

static int
get_distance_between(struct Path *path, struct TrackPosition *src, struct TrackPosition *dest) {
  int dist = -src->offset + dest->offset;

  int pos1_index = 0;
  // find pos1 index in path
  for (int i = path->nodes_len - 1; i >= 0; --i) {
    if (src->node == path->nodes[i]) {
      pos1_index = i;
    }
  }

  // find distance from pos1 to pos2
  for (int i = pos1_index; i >= 0; --i) {
    struct TrackNode *node = path->nodes[i];
    int edge_dir = path->directions[i];

    if (node == dest->node) {
      break;
    }

    dist += node->edge[edge_dir].dist;
  }

  return dist;
}

static int get_travel_time_between(
    struct Path *path,
    struct TrainState *state,
    struct TrackPosition *pos1,
    struct TrackPosition *pos2
) {
  int dist = get_distance_between(path, pos1, pos2);
  return dist / TRAINSET_MEASURED_SPEEDS[state->train][state->speed];
}

static struct TrackPosition plan_get_stopping_pos(struct Plan *plan, struct TrainState *state) {
  struct Path *path = &plan->path;
  // distance from train position
  int stopping_offset = path->distance - state->last_known_pos.offset + plan->dest.offset -
                        TRAINSET_STOPPING_DISTANCES[state->train][state->speed];

  struct TrackNode *last_node = NULL;
  int offset_from_last_node = stopping_offset;
  // traverse path to see which node is before the stopping_offset
  for (int i = path->nodes_len - 1; i >= 0; ++i) {
    struct TrackNode *cur_node = path->nodes[i];
    int edge_dir = path->directions[i];

    if (offset_from_last_node < cur_node->edge[edge_dir].dist) {
      last_node = cur_node;
      break;
    }
  }

  // our offset is not covered by the edge after the last node
  if (last_node == NULL) {
    // set to last node
    last_node = plan->path.nodes[plan->path.nodes_len - 1];
  }

  struct TrackPosition pos = {.node = last_node, .offset = offset_from_last_node};
  return pos;
}

// static void *set_switches_to_dest() {}

static int current_train = 0;

static void handle_begin_route_request(
    int train_tid,
    int clock_server,
    struct TrainState *train_states,
    struct TrainRouterBeginRouteRequest *req
) {
  current_train = req->train;

  struct TrainState *trainstate = &train_states[trainset_get_train_index(req->train)];
  trainstate_init(trainstate, req->train);

  struct Plan *plan = &trainstate->plan;
  plan->state = PLAN_ACCELERATING;
  plan->state_update_time = Time(clock_server);
  plan->dest.node = track_get_sensor(req->sensor);
  plan->dest.offset = req->sensor_offset;
  plan->time_to_stop = INT_MAX;

  // begin acceleration from stopped position
  trainstate->speed = req->speed;
  TrainSetSpeed(train_tid, req->train, req->speed);
}

static void handle_update_sensors_request(
    int train_tid,
    int clock_server,
    struct TrainState *train_states,
    struct TrainRouterUpdateSensorsRequest *req
) {
  // no train being routed
  if (current_train == 0) {
    return;
  }

  // currently can only route one train at a time.
  struct TrainState *trainstate = &train_states[trainset_get_train_index(current_train)];
  struct Plan *plan = &trainstate->plan;

  int triggered_sensor = 0;
  for (int i = 0; i < TRAINSET_SENSORS_LEN; ++i) {
    // TODO: support for multiple trains.
    // for now we stop as soon as we see a sensor has been triggered
    // need to associate sensor with train for multiple trains.
    if (req->sensors[i]) {
      triggered_sensor = i;
      break;
    }
  }

  trainstate->last_known_pos.node = &track[triggered_sensor];
  trainstate->last_known_pos.offset = 0;

  trainstate->direction = trainstate_get_direction(trainstate);

  switch (plan->state) {
    case PLAN_NONE:
      break;
    case PLAN_ACCELERATING:
      // check if time since start of acceleration has been long enough
      if (Time(clock_server) - plan->state_update_time >= ACCELERATION_DURATION) {
        // TODO: should figure out if the dest offset is past another sensor, if it is we should
        // move dest.node to be the last sensor. that is, the sensor right before dest.node + offset
        plan->path = get_shortest_path(trainstate, plan->dest.node);

        path_set_switches(&plan->path, train_tid);
        plan->state = PLAN_WAITING_TO_STOP;
        plan->state_update_time = Time(clock_server);

        // update stopping point
        plan->stopping_pos = plan_get_stopping_pos(plan, trainstate);
        plan->time_to_stop = Time(clock_server) +
                             get_travel_time_between(
                                 &plan->path, trainstate, &trainstate->last_known_pos, &plan->dest
                             );

        // the first node in our path is always a sensor.
        plan->last_sensor_index = plan->path.nodes_len - 1;
      }
      break;
    case PLAN_CALCULATING_PATH:
      // TODO: move to different task?
      break;
    case PLAN_WAITING_TO_STOP: {
      // update terminal with time to reach next sensor
      // search path for previous sensor triggered
      int next_sensor_index = get_next_sensor(plan, &plan->path);

      if (next_sensor_index == -1) {
        // should never happen
        printf("train_router: ERROR EXPECTED A NEXT SENSOR\r\n");
      }

      // update time to stop
      plan->time_to_stop =
          Time(clock_server) + get_travel_time_between(
                                   &plan->path, trainstate, &trainstate->last_known_pos, &plan->dest
                               );
    }

    // TODO: maybe we should stop here instead of in the timer request?

    break;
  }
  // get next sensor and distance to next sensor
  // get stopping distance (sensor and offset)
}

void handle_tick(int train_tid, int clock_server, struct TrainState *train_states) {
  struct TrainState *trainstate = &train_states[current_train];
  struct Plan *plan = &trainstate->plan;

  // TODO: we want to check every train eventually not just one train to see if we should stop it
  if (current_train != 0 && plan->state == PLAN_WAITING_TO_STOP) {
    // not time to stop yet
    if (Time(clock_server) < plan->time_to_stop) {
      return;
    }

    // stop the train
    TrainSetSpeed(train_tid, trainstate->train, 0);
    trainstate->plan.state = PLAN_NONE;
    // reset
    current_train = 0;
  }
}

void train_router_tick_notifier() {
  int train_router = MyParentTid();
  int clock_server = WhoIs("clock_server");

  struct TrainRouterRequest req = {.type = TRAIN_ROUTER_TICK};

  while (true) {
    Delay(clock_server, 1);
    Send(train_router, (const char *) &req, sizeof(req), NULL, 0);
  }
}

void train_router_task() {
  RegisterAs("train_router");

  int train_tid = WhoIs("train");
  int clock_server = WhoIs("clock_server");

  init_measured_speeds();

  tracka_init(track);

  struct TrainState train_states[TRAINSET_NUM_TRAINS];

  int tid;
  struct TrainRouterRequest req;

  current_train = 0;

  struct TrainState fake_train_state = {
      .plan =
          {
              .state = PLAN_WAITING_TO_STOP,
              .state_update_time = Time(clock_server),
              .dest = {.node = &track[113], .offset = 3},
              .last_sensor_index = 7,
          },

      .last_known_pos = {.node = &track[3], .offset = 0},
      .direction = TRAINDIR_FORWARD,
      .speed = 12,
      .train = 2
  };

  struct TrackNodeQueue queue;
  track_node_queue_init(&queue, track);

  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    // printf("received\r\n");
    // struct Path path = get_shortest_path(&fake_train_state, fake_train_state.plan.dest.node);
    // fake_train_state.plan.path = path;
    // // path_set_switches(&path, train_tid);
    // printf("done shortest path\r\n");

    // for (int i = path.nodes_len - 1; i >= 0; --i) {
    //   struct TrackNode *node = path.nodes[i];
    //   int dir = path.directions[i];
    //   int dist = path.nodes[i]->edge[dir].dist;

    //   printf("node %s dir: %d dist: %d\r\n", node->name, path.directions[i], dist);
    // }

    // struct TrackNode *a4 = path.nodes[7];
    // struct TrackNode *br15 = path.nodes[5];

    // struct TrackPosition src = {.node = a4, .offset = 3};
    // struct TrackPosition dest = {.node = br15, .offset = 5};

    // printf("from %s to %s: %d\r\n", a4->name, br15->name, get_distance_between(&path, &src,
    // &dest));

    switch (req.type) {
      case TRAIN_ROUTER_BEGIN_ROUTE:
        handle_begin_route_request(train_tid, clock_server, train_states, &req.begin_route_req);
        Reply(tid, NULL, 0);
        break;
      case TRAIN_ROUTER_UPDATE_SENSORS:
        handle_update_sensors_request(train_tid, clock_server, train_states, &req.update_sens_req);
        Reply(tid, NULL, 0);
        // should make another server that will notify this server of periodic time changes.
        // then this server should check if enough time has passed for the train to stop.
        break;
      case TRAIN_ROUTER_TICK:
        handle_tick(train_tid, clock_server, train_states);
        Reply(tid, NULL, 0);
    }
  }

  // given a request
  // - wait for time to accelerate to constant speed
  // - after constant speed, wait for sensor trip
  // - after sensor trip, find route to destination.
  // - find time to stopping destination by (d - stopping distance) / v = t
  // - total time to destination = t + stopping time
  // - keep recalculating time to destination.
  // - after last sensor hit, stop train after t.

  // after sensor trip, find route to destination
  // loop through all nodes for all sensors and wait for next sensor trip, keep track of current
  // sensor
  Exit();
}

void TrainRouterRouteTrain(int tid, int train, int speed, char *sensor, int offset) {
  struct TrainRouterRequest req = {
      .type = TRAIN_ROUTER_BEGIN_ROUTE,
      .begin_route_req = {.sensor = sensor, .train = train, .speed = speed, .sensor_offset = offset}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainRouterUpdateSensors(int tid, bool *sensors) {
  struct TrainRouterRequest req = {
      .type = TRAIN_ROUTER_UPDATE_SENSORS, .update_sens_req = {.sensors = sensors}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
