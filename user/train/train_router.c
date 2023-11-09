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
#include "user/terminal/terminal_task.h"

#define TRAIN_SPEED_MAX 14

#define FIXED_POINT_MULTIPLIER 100

typedef int FixedPointInt;

inline FixedPointInt fixed_point_int_from(int val) {
  return val * FIXED_POINT_MULTIPLIER;
}

inline int fixed_point_int_get(int val) {
  return val / FIXED_POINT_MULTIPLIER;
}

// add 1 to include 0
static FixedPointInt TRAINSET_MEASURED_SPEEDS[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
static int TRAINSET_STOPPING_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
static int TRAINSET_STOPPING_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];

static void init_measured_speeds() {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    TRAINSET_MEASURED_SPEEDS[i][0] = 0;
    TRAINSET_STOPPING_DISTANCES[i][0] = 0;
    TRAINSET_STOPPING_TIMES[i][0] = 0;
  }

  // mm/s
  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(77)][14] = 185;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(77)][14] = 0;

  // TRAIN 58
  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(58)][13] = 510;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(58)][13] = 980;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(58)][10] = 308;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(58)][10] = 440;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(58)][7] = 334;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(58)][7] = 145;

  // TRAIN 54
  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][13] = 599;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][13] = 940;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][12] = 597;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][12] = 955;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][12] = 585;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][12] = 860;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][11] = 537;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][11] = 780;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][10] = 483;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][10] = 705;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][9] = 432;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][9] = 635;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][8] = 385;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][8] = 558;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][7] = 315;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][7] = 490;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][6] = 285;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][6] = 400;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][5] = 231;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][5] = 320;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][4] = 174;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][4] = 250;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][3] = 127;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][3] = 160;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][2] = 75;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][2] = 80;

  // TRAIN 47
  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(47)][13] = 566;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(47)][13] = 920;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(47)][10] = 477;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(47)][10] = 885;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(47)][7] = 351;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(47)][7] = 615;
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
static const int ACCELERATION_DURATION = 500;

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
  int next_sensor_eta;
};

struct TrainState {
  int train;
  int train_index;

  enum TrainDirection direction;
  struct TrackPosition last_known_pos;
  int last_known_pos_time;

  struct Plan plan;

  int speed;
};

void trainstate_init(struct TrainState *state, int train, int speed) {
  state->direction = TRAINDIR_UNKNOWN;
  state->last_known_pos_time = 0;
  state->speed = speed;
  state->train = train;
  state->train_index = trainset_get_train_index(train);
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
static FixedPointInt trainstate_get_current_offset(struct TrainState *state, int current_time) {
  // velocity * time since last sensor = offset
  return TRAINSET_MEASURED_SPEEDS[state->train_index][state->speed] * current_time;
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

static struct TrackNodePriorityQueue queue;

// Dijikstra's algorithm
static struct Path get_shortest_path(struct TrainState *state, struct TrackNode *dest) {
  int dist[TRACK_MAX] = {0};
  // initially filled with null
  struct TrackNode *prev[TRACK_MAX] = {0};
  int directions[TRACK_MAX] = {0};

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
  // mm
  int dist = get_distance_between(path, pos1, pos2);

  // in ticks (per 10ms)
  return fixed_point_int_from(dist) / TRAINSET_MEASURED_SPEEDS[state->train_index][state->speed];
}

static struct TrackPosition plan_get_stopping_pos(struct Plan *plan, struct TrainState *state) {
  struct Path *path = &plan->path;
  // distance from train position
  int stopping_offset = path->distance - state->last_known_pos.offset + plan->dest.offset -
                        TRAINSET_STOPPING_DISTANCES[state->train_index][state->speed];

  struct TrackNode *last_node = NULL;
  int offset_from_last_node = stopping_offset;
  // traverse path to see which node is before the stopping_offset
  for (int i = path->nodes_len - 1; i >= 0; --i) {
    struct TrackNode *cur_node = path->nodes[i];
    int edge_dir = path->directions[i];
    int edge_dist = cur_node->edge[edge_dir].dist;

    if (offset_from_last_node < edge_dist) {
      last_node = cur_node;
      break;
    }

    offset_from_last_node -= edge_dist;
  }

  // our offset is not covered by the edge after the last node
  if (last_node == NULL) {
    // set to last node
    last_node = plan->path.nodes[0];
  }

  struct TrackPosition pos = {.node = last_node, .offset = offset_from_last_node};
  return pos;
}

// static void *set_switches_to_dest() {}

static int current_train = 0;

static void handle_begin_route_request(
    int terminal,
    int train_tid,
    int clock_server,
    struct TrainState *train_states,
    struct TrainRouterBeginRouteRequest *req
) {
  current_train = req->train;

  struct TrainState *trainstate = &train_states[trainset_get_train_index(req->train)];
  trainstate_init(trainstate, req->train, req->speed);

  struct Plan *plan = &trainstate->plan;
  plan->state = PLAN_ACCELERATING;
  plan->state_update_time = Time(clock_server);
  plan->dest.node = track_get_sensor(req->sensor);
  plan->dest.offset = req->sensor_offset;
  plan->time_to_stop = INT_MAX;
  plan->next_sensor_eta = 0;

  // begin acceleration
  TrainSetSpeed(train_tid, req->train, req->speed);

  TerminalLogPrint(terminal, "Begin acceleration for train %d\r\n", req->train);
}

static void handle_update_sensors_request(
    int terminal,
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

  int triggered_sensor = -1;
  for (int i = 0; i < TRAINSET_SENSORS_LEN; ++i) {
    // TODO: support for multiple trains.
    // for now we stop as soon as we see a sensor has been triggered
    // need to associate sensor with train for multiple trains.
    if (req->sensors[i]) {
      triggered_sensor = i;
      break;
    }
  }

  if (triggered_sensor == -1) {
    return;
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
        // should also figure out negative offset and convert it to positive offset.
        plan->path = get_shortest_path(trainstate, plan->dest.node);

        struct Path *path = &plan->path;

        TerminalLogPrint(terminal, "Calculated path, distance: %d", path->distance);
        TerminalLogPrint(terminal, "Total of %d nodes in path", path->nodes_len);

        path_set_switches(&plan->path, train_tid);
        plan->state = PLAN_WAITING_TO_STOP;
        plan->state_update_time = Time(clock_server);

        // update stopping point
        plan->stopping_pos = plan_get_stopping_pos(plan, trainstate);
        TerminalLogPrint(
            terminal,
            "Stopping point node %s with offset %d",
            plan->stopping_pos.node->name,
            plan->stopping_pos.offset
        );

        plan->time_to_stop =
            Time(clock_server) +
            get_travel_time_between(
                &plan->path, trainstate, &trainstate->last_known_pos, &plan->stopping_pos
            );
        TerminalLogPrint(
            terminal, "Initial estimated time to stop (ticks): %d", plan->time_to_stop
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
      plan->last_sensor_index = get_next_sensor(plan, &plan->path);

      if (plan->next_sensor_eta != 0) {
        TerminalLogPrint(
            terminal,
            "Estimated sensor ETA error (ticks): %d",
            Time(clock_server) - plan->next_sensor_eta
        );
      }

      int next_sensor_index = get_next_sensor(plan, &plan->path);
      int travel_time = get_travel_time_between(
          &plan->path, trainstate, &trainstate->last_known_pos, &plan->stopping_pos
      );
      TerminalUpdateStatus(terminal, "Time to stopping distance (ticks): %d", travel_time);

      TerminalLogPrint(
          terminal,
          "Distance from %s (last known position) to %s (dest): %d",
          trainstate->last_known_pos.node->name,
          plan->dest.node->name,
          get_distance_between(&plan->path, &trainstate->last_known_pos, &plan->dest)
      );

      // update time to stop
      plan->time_to_stop = Time(clock_server) + travel_time;
      if (next_sensor_index != -1) {
      } else {
        TerminalLogPrint(terminal, "No sensors left until destination.");
      }

      struct TrackNode *sensor_node = plan->path.nodes[next_sensor_index];
      struct TrackPosition sensor_pos = {.node = sensor_node, .offset = 0};
      int next_sensor_travel_time = get_travel_time_between(
          &plan->path, trainstate, &trainstate->last_known_pos, &sensor_pos
      );
      plan->next_sensor_eta = Time(clock_server) + next_sensor_travel_time;

      TerminalLogPrint(
          terminal,
          "Next sensor: %s, %d ticks away, %dmm away",
          sensor_node->name,
          next_sensor_travel_time,
          get_distance_between(&plan->path, &trainstate->last_known_pos, &sensor_pos)
      );

      // TODO: handle skipped sensors.
    }

    // TODO: maybe we should stop here instead of in the timer request?

    break;
  }
  // get next sensor and distance to next sensor
  // get stopping distance (sensor and offset)
}

void handle_tick(int terminal, int train_tid, int clock_server, struct TrainState *train_states) {
  struct TrainState *trainstate = &train_states[trainset_get_train_index(current_train)];
  struct Plan *plan = &trainstate->plan;

  // TODO: we want to check every train eventually not just one train to see if we should stop it
  if (current_train != 0 && plan->state == PLAN_WAITING_TO_STOP) {
    // not time to stop yet
    if (Time(clock_server) < plan->time_to_stop) {
      return;
    }

    TerminalLogPrint(terminal, "Stopping train %d", trainstate->train, plan->time_to_stop);

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
  int terminal = WhoIs("terminal");

  init_measured_speeds();

  // tracka_init(track);
  trackb_init(track);

  track_node_priority_queue_init(&queue, track);

  struct TrainState train_states[TRAINSET_NUM_TRAINS];

  int tid;
  struct TrainRouterRequest req;

  current_train = 0;

  Create(TRAIN_TASK_PRIORITY, train_router_tick_notifier);

  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case TRAIN_ROUTER_BEGIN_ROUTE:
        handle_begin_route_request(
            terminal, train_tid, clock_server, train_states, &req.begin_route_req
        );
        Reply(tid, NULL, 0);
        break;
      case TRAIN_ROUTER_UPDATE_SENSORS:
        handle_update_sensors_request(
            terminal, train_tid, clock_server, train_states, &req.update_sens_req
        );
        Reply(tid, NULL, 0);
        // should make another server that will notify this server of periodic time changes.
        // then this server should check if enough time has passed for the train to stop.
        break;
      case TRAIN_ROUTER_TICK:
        handle_tick(terminal, train_tid, clock_server, train_states);
        Reply(tid, NULL, 0);
    }
  }

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
