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

enum PlanState {
  PLAN_ACCELERATING,
  PLAN_CALCULATING_PATH,
  PLAN_SWITCHES_SET,
  PLAN_WAITING_TO_STOP
};

struct Plan {
  enum PlanState state;
  int state_update_time;

  struct Path path;
  struct TrackPosition dest;
  struct TrackPosition stopping_point;
};

struct TrainState {
  int train;

  enum TrainDirection direction;
  struct TrackPosition last_known_pos;
  int last_known_pos_time;

  struct Plan plan;

  int speed;
  bool moving;
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

static struct TrackNode *trackstate_get_next_sensor(struct TrainState *state) {
  struct TrackNode *node = state->last_known_pos.node;
  int dist = 0;

  while (node->type != NODE_SENSOR || node == state->last_known_pos.node) {
    dist += node->edge->dist;
    node = node->edge->dest;

    if (node->type == NODE_EXIT) {
      return NULL;
    }
  }

  return node;
}

struct Path {
  // nodes starting from destination
  struct TrackNode *nodes[TRACK_MAX];
  int directions[TRACK_MAX];
  int nodes_len;
  int distance;
};

static void path_set_switches(struct Path *path, int train_tid) {
  for (int i = path->nodes_len - 1; i >= 0; --i) {
    struct TrackNode *node = path->nodes[i];

    // switch
    if (node->type == NODE_BRANCH) {
      printf("switch %s dir: %d\r\n", node->name, path->directions[i]);

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

// static void *set_switches_to_dest() {}

static int current_train = 0;

static void handle_begin_route_request(
    int train_tid,
    int clock_server,
    struct TrainState *train_states,
    struct TrainRouterBeginRouteRequest *req
) {
  struct TrainState *trainstate = &train_states[trainset_get_train_index(req->train)];
  struct Plan plan = {
      .state = PLAN_ACCELERATING,
      .state_update_time = Time(clock_server),
      .dest = {.node = req->sensor, .offset = req->sensor_offset}
  };

  current_train = req->train;

  trainstate_init(trainstate, req->train);

  trainstate->dest.node = req->sensor;
  trainstate->dest.offset = req->sensor_offset;

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

  // position is now known
  if (trainstate->direction == TRAINDIR_UNKNOWN) {
    // compute path and set switches on track
    struct TrackNodeQueue path_nodes;
    track_node_queue_init(&path_nodes, track);

    struct Path path = get_shortest_path(trainstate, trainstate->dest.node);

    while (!track_node_queue_empty(&path_nodes)) {
      struct TrackNode *node = track_node_queue_poll(&path_nodes);

      printf("node %s\r\n", node->name);
    }
  }

  trainstate->last_known_pos.node = &track[triggered_sensor];
  trainstate->last_known_pos.offset = 0;

  // TODO: wait for train time to accelerate, must manually set speed before hand for now.

  // get next sensor and distance to next sensor
  // get stopping distance (sensor and offset)
  trainstate->direction = trainstate_get_direction(trainstate);
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

  // currently routing a train
  bool routing = false;

  struct TrainState fake_train_state = {
      .dest = {.node = &track[113], .offset = 3},
      .last_known_pos = {.node = &track[3], .offset = 0},
      .direction = TRAINDIR_FORWARD,
      .moving = true,
      .speed = 12,
      .start_accel_time = 0,
      .train = 2
  };

  struct TrackNodeQueue queue;
  track_node_queue_init(&queue, track);

  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    printf("received\r\n");
    struct Path path = get_shortest_path(&fake_train_state, fake_train_state.dest.node);
    path_set_switches(&path, train_tid);
    printf("done shortest path\r\n");

    for (int i = path.nodes_len - 1; i >= 0; --i) {
      struct TrackNode *node = path.nodes[i];

      printf("node %s dir: %d\r\n", node->name, path.directions[i]);
    }

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
