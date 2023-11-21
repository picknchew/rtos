#include "train_manager.h"

#include "selected_track.h"
#include "syscall.h"
#include "track_position.h"
#include "train_planner.h"
#include "train_sensor_notifier.h"
#include "trainset.h"
#include "trainset_calib_data.h"
#include "trainset_task.h"
#include "user/server/clock_server.h"
#include "user/server/name_server.h"
#include "user/terminal/terminal_task.h"
#include "util.h"

enum TrainState {
  PLAN_BEGIN,
  SHORT_MOVE,
  ACCELERATING,
  CONSTANT_VELOCITY,
  DECELERATING,
  STOPPED,
  WAITING_FOR_INITIAL_POS
};

struct ShortMove {
  int start_time;
  int duration;
};

struct Train {
  bool active;
  bool started;

  int train;
  int train_index;

  enum TrainState state;

  int decel_begin_time;
  int decel_duration;

  int last_sensor_index;
  int last_switch_index;

  int move_start_time;
  int move_duration;
  int move_stop_time;

  struct RoutePlan plan;
  // index of current path in route plan
  int path_index;
  // index of next node in total path
  int path_node_index;

  struct TrackPosition current_pos;
  int current_pos_update_time;
  struct TrackPosition last_known_pos;
  int last_known_pos_time;

  int speed;
};

// TODO: if train hits any sensor after a node in the path, this won't work because it cannot find
// the destinaton. theoretically, this should never happen because the train should always stop at
// the destination.
static int
get_distance_between(struct Path *path, struct TrackPosition *src, struct TrackPosition *dest) {
  int dist = -src->offset + dest->offset;

  int pos1_index = 0;
  // find pos1 index in path
  for (int i = path->nodes_len - 1; i >= 0; --i) {
    if (src->node == path->nodes[i] || src->node == path->nodes[i]->reverse) {
      pos1_index = i;
    }
  }

  // find distance from pos1 to pos2
  for (int i = pos1_index; i >= 0; --i) {
    struct TrackNode *node = path->nodes[i];
    int edge_dir = path->directions[i];

    if (node == dest->node || node == dest->node->reverse) {
      break;
    }

    if (edge_dir == DIR_REVERSE) {
      continue;
    }

    dist += node->edge[edge_dir].dist;
  }

  return dist;
}

static void train_log(int terminal, struct Train *train) {
  TerminalLogPrint(terminal, "=====Train %d=====", train->train);
  TerminalLogPrint(terminal, "active: %d", train->active);
  TerminalLogPrint(terminal, "started: %d", train->started);
  TerminalLogPrint(terminal, "train: %d", train->active);

  switch (train->state) {
    case PLAN_BEGIN:
      TerminalLogPrint(terminal, "state: PLAN_BEGIN");
      break;
    case SHORT_MOVE:
      TerminalLogPrint(terminal, "state: SHORT_MOVE");
      break;
    case ACCELERATING:
      TerminalLogPrint(terminal, "state: ACCELERATING");
      break;
    case CONSTANT_VELOCITY:
      TerminalLogPrint(terminal, "state: CONSTANT_VELOCITY");
      break;
    case DECELERATING:
      TerminalLogPrint(terminal, "state: DECELERATING");
      break;
    case STOPPED:
      TerminalLogPrint(terminal, "state: STOPPED");
      break;
    case WAITING_FOR_INITIAL_POS:
      TerminalLogPrint(terminal, "state: WAITING_FOR_INITIAL_POS");
      break;
  }

  if (train->active) {
    TerminalLogPrint(terminal, "decel_begin_time: %d", train->decel_begin_time);
    TerminalLogPrint(terminal, "decel_duration: %d", train->decel_duration);
    TerminalLogPrint(terminal, "last_sensor_index: %d", train->last_sensor_index);
    TerminalLogPrint(terminal, "move_start_time: %d", train->move_start_time);
    TerminalLogPrint(terminal, "move_duration: %d", train->move_duration);
    TerminalLogPrint(terminal, "move_stop_time %d", train->move_stop_time);
    TerminalLogPrint(
        terminal,
        "current_pos: %s, offset: %d",
        train->current_pos.node->name,
        train->current_pos.offset
    );
    TerminalLogPrint(terminal, "current_pos_update_time: %d", train->current_pos_update_time);
  }
}

void train_init(struct Train *train, int train_num) {
  train->active = false;
  train->started = false;

  train->train = train_num;
  train->train_index = trainset_get_train_index(train_num);

  train->state = WAITING_FOR_INITIAL_POS;
  train->move_start_time = 0;
  train->move_duration = 0;
  train->move_stop_time = 0;

  train->last_sensor_index = 0;
  train->last_switch_index = 0;

  train->plan.path_found = false;
  train->path_index = 0;
  train->path_node_index = 0;

  train->current_pos_update_time = 0;
  train->last_known_pos_time = 0;

  train->speed = 0;
}

static inline int get_constant_velocity_travel_time(struct Train *train, int dist) {
  // in ticks (per 10ms)
  return fixed_point_int_from(dist) / TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed];
}

static int get_constant_velocity_travel_time_between(
    struct Train *train,
    struct TrackPosition *pos1,
    struct TrackPosition *pos2
) {
  // mm
  int dist = get_distance_between(&train->plan.path, pos1, pos2);

  return get_constant_velocity_travel_time(train, dist);
}

static struct TrackPosition
plan_get_stopping_pos(struct Train *train, struct Path *path, struct TrackPosition *dest) {
  // distance from train position
  int stopping_offset = get_distance_between(path, &train->current_pos, dest);

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
    last_node = path->nodes[0];
  }

  struct TrackPosition pos = {.node = last_node, .offset = offset_from_last_node};
  return pos;
}

enum TrainManagerRequestType { TRAIN_MANAGER_UPDATE_SENSORS, TRAIN_MANAGER_TICK };

struct TrainManagerUpdateSensorsRequest {
  bool *sensors;
};

struct TrainManagerRequest {
  enum TrainManagerRequestType type;

  union {
    struct TrainManagerUpdateSensorsRequest update_sens_req;
  };
};

inline FixedPointInt get_train_accel(struct Train *train) {
  // assuming constant velocity
  return TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed] /
         TRAINSET_ACCEL_TIMES[train->train_index][train->speed];
}

// returns time to move distance for a full acceleration to constant velocity move
int get_move_time(struct Train *train, int distance) {
  distance -= TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed];

  int constant_velocity_time = get_constant_velocity_travel_time(
      train, (distance - TRAINSET_ACCEL_DISTANCES[train->train_index][train->speed])
  );

  return TRAINSET_ACCEL_TIMES[train->train_index][train->speed] + constant_velocity_time;
}

inline int get_dist_travelled_accel(FixedPointInt initial_velocity, int accel, int time) {
  // d = v0 * t + 1/2 * at^2
  return fixed_point_int_get(initial_velocity * time + (accel * time * time) / 2);
}

inline int get_dist_constant_velocity(FixedPointInt velocity, int time) {
  return fixed_point_int_get(velocity * time);
}

// distance moved since last current pos update
int get_train_dist_travelled_full_move(struct Train *train, int time) {
  int total_move_duration = time - train->move_start_time;
  int last_update_total_move_duration = train->current_pos_update_time - train->move_start_time;

  // 3 scenarios:
  // - position updated in the middle of constant velocity.
  // - position updated in the middle of acceleration
  //   - still accelerating
  //   - reached constant velocity.

  // position updated in the middle of constant velocity.
  if (train->last_known_pos_time - train->move_start_time >
      TRAINSET_ACCEL_TIMES[train->train_index][train->speed]) {
    // last pos update happened after constant velocity was reached.
    return get_dist_constant_velocity(
        TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed],
        time - train->last_known_pos_time
    );
  }

  // assume constant acceleration
  int accel = get_train_accel(train);

  // position updated in the middle of acceleration
  // still accelerating
  if (time - train->move_start_time <= TRAINSET_ACCEL_TIMES[train->train_index][train->speed]) {
    // get distance from last known position
    return get_dist_travelled_accel(0, accel, time - train->move_start_time) -
           get_dist_travelled_accel(0, accel, time - train->last_known_pos_time);
  }

  int begin_constant_velocity_time =
      train->move_start_time + TRAINSET_ACCEL_TIMES[train->train_index][train->speed];

  // reached constant speed
  // full acceleration dist since move began - distance travelled since move began to last known pos
  int accel_dist =
      get_dist_travelled_accel(0, accel, begin_constant_velocity_time - train->move_start_time) -
      get_dist_travelled_accel(0, accel, train->last_known_pos_time - train->move_start_time);

  // constant velocity duration
  int constant_velocity_duration = time - (begin_constant_velocity_time);
  int constant_velocity_dist = get_dist_constant_velocity(
      TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed], constant_velocity_duration
  );

  return accel_dist + constant_velocity_dist;
}

int get_train_dist_travelled_decel(struct Train *train, int time) {
  // assuming constant deceleration
  FixedPointInt decel = -TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed] /
                        TRAINSET_DECEL_TIMES[train->train_index][train->speed];
  // two scnenarios:
  // - last known pos is from while the train was accelerating or going at
  //   constant velocity
  // - last known pos is from while the train was decelerating.

  // last known pos is from while the train was accelerating or going at constant velocity
  if (train->last_known_pos_time < train->decel_begin_time) {
    // distance travelled while at constant velocity or accelerating
    int accel_dist = get_train_dist_travelled_full_move(train, train->decel_begin_time);
    int decel_dist = get_dist_travelled_accel(
        TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed],
        decel,
        time - train->decel_begin_time
    );

    return accel_dist + decel_dist;
  }

  // last known pos is from while the train was decelerating.
  int decel_dist_to_known_pos = get_dist_travelled_accel(
      TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed],
      decel,
      train->last_known_pos_time - train->decel_begin_time
  );
  int total_decel_dist = get_dist_travelled_accel(
      TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed],
      decel,
      time - train->decel_begin_time
  );

  // assuming constant deceleration from max velocity to 0
  return total_decel_dist - decel_dist_to_known_pos;
}

void update_train_pos(int clock_server, struct Train *train) {
  if (train->state == STOPPED || train->state == WAITING_FOR_INITIAL_POS ||
      train->state == PLAN_BEGIN) {
    return;
  }

  int time = Time(clock_server);

  struct SimplePath current_path = train->plan.paths[train->path_index];
  // destination node of simple path
  struct TrackNode *current_path_dest = train->plan.path.nodes[current_path.end_index];
  int current_path_dest_dir = train->plan.path.directions[current_path.end_index];
  struct TrackPosition current_path_dest_pos = {
      .node = current_path_dest, .offset = current_path_dest->edge[current_path_dest_dir].dist
  };

  if (current_path_dest == train->plan.dest.node) {
    current_path_dest_pos.offset = train->plan.dest.offset;
  }

  switch (train->state) {
    case SHORT_MOVE:
      if (train->move_start_time + train->move_duration <= time) {
        train->current_pos = current_path_dest_pos;
        train->current_pos_update_time = time;
      }
      break;
    case ACCELERATING:
      train->current_pos = track_position_add(
          train->last_known_pos,
          &train->plan.path,
          get_train_dist_travelled_full_move(train, time - train->move_start_time)
      );
      train->current_pos_update_time = time;

      if (train->move_start_time + TRAINSET_ACCEL_TIMES[train->train_index][train->speed] <= time) {
        // we've reached constant velocity
        train->state = CONSTANT_VELOCITY;
      }
      break;
    case CONSTANT_VELOCITY:
      train->current_pos = track_position_add(
          train->last_known_pos,
          &train->plan.path,
          get_train_dist_travelled_full_move(train, time - train->move_start_time)
      );
      train->current_pos_update_time = time;

      if (train->move_start_time + train->move_duration <= time) {
        train->current_pos = current_path_dest_pos;
      }
      break;
    case DECELERATING:
      // assume constant deceleration
      train->current_pos = track_position_add(
          train->current_pos, &train->plan.path, get_train_dist_travelled_decel(train, time)
      );
      train->current_pos_update_time = time;
      break;
    default:
      break;
  }
}

bool has_reached_dest(struct Train *train, struct TrackPosition *dest) {
  return get_distance_between(&train->plan.path, &train->current_pos, dest) <= 0;
}

static int get_next_sensor_index(struct Train *train) {
  int last_sensor_index =
      train->last_sensor_index == -1 ? train->plan.path.nodes_len : train->last_sensor_index;

  for (int i = train->last_sensor_index - 1; i >= 0; --i) {
    struct TrackNode *node = train->plan.path.nodes[i];

    if (node->type == NODE_SENSOR) {
      return i;
    }
  }

  return -1;
}

static int get_next_switch_index(struct Train *train) {
  int last_switch_index =
      train->last_switch_index == -1 ? train->plan.path.nodes_len : train->last_switch_index;

  for (int i = last_switch_index - 1; i >= 0; --i) {
    struct TrackNode *node = train->plan.path.nodes[i];

    if (node->type == NODE_BRANCH) {
      return i;
    }
  }

  return -1;
}

static const int REVERSE_OVERSHOOT_DIST = 200;

void route_train_randomly(int terminal, int train_planner, struct Train *train) {
  struct TrackPosition rand_dest = track_position_random();
  train->plan = CreatePlan(train_planner, &train->current_pos, &rand_dest);

  while (!train->plan.path_found) {
    TerminalLogPrint(
        terminal,
        "No path to %s from %s, trying to find another destination.",
        rand_dest.node->name,
        train->current_pos.node->name
    );
    rand_dest = track_position_random();
    train->plan = CreatePlan(train_planner, &train->current_pos, &rand_dest);
  }

  train->path_index = 0;
  train->path_node_index = 0;
  // if the train is currently on a sensor, then the path starts at a sensor, and we set
  // last sensor triggered to the first node.
  train->last_sensor_index = -1;
  train->last_switch_index = -1;
  train->state = PLAN_BEGIN;

  TerminalLogPrint(terminal, "Routing train %d to %s.", train->train, rand_dest.node->name);
}

static const int FLIP_SWITCH_DIST = 300;

static void handle_tick(
    int terminal,
    int train_tid,
    int train_planner,
    int clock_server,
    struct Train *train_states
) {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    struct Train *train = &train_states[i];

    // trains should always have a route plan. if they do not have a route plan they are not active.
    if (!train->active) {
      continue;
    }

    update_train_pos(clock_server, train);

    struct Path *path = &train->plan.path;

    // TODO:
    // - make sure we are within reservation range ^

    // when we are close enough to the next switch, activate it.
    int next_switch_index = get_next_switch_index(train);
    // we check if nexxt switch is equal to last switch in case we already activated
    // the switch previously.
    if (next_switch_index != -1 && next_switch_index != train->last_switch_index) {
      struct TrackNode *next_switch = path->nodes[next_switch_index];
      struct TrackPosition next_switch_pos = {.node = next_switch, .offset = 0};

      TerminalLogPrint(
          terminal, "we're in %d", get_distance_between(path, &train->current_pos, &next_switch_pos)
      );

      // TODO: check for reservation
      if (get_distance_between(path, &train->current_pos, &next_switch_pos) <= FLIP_SWITCH_DIST) {
        TrainSetSwitchDir(train_tid, next_switch->num, path->directions[next_switch_index]);
        train->last_switch_index = next_switch_index;
      }
    }

    // location updates and reservations.
    int next_sensor_index = get_next_sensor_index(train);
    if (next_sensor_index != -1) {
      struct TrackNode *next_sensor = path->nodes[next_sensor_index];
      struct TrackPosition next_sensor_pos = {.node = next_sensor, .offset = 0};

      // TODO: update time to next sensor and distance
      // int dist = get_distance_between(&train->plan.path, &train->current_pos, &next_sensor_pos);
      // int time

      // we only update last_sensor_index when it is actually triggered.
    }

    struct SimplePath current_path = train->plan.paths[train->path_index];
    // destination node of simple path
    struct TrackNode *current_path_dest = train->plan.path.nodes[current_path.end_index];
    int current_path_dest_dir = train->plan.path.directions[current_path.end_index];
    struct TrackPosition current_path_dest_pos = {
        .node = current_path_dest,
        .offset = current_path_dest->edge[current_path_dest_dir].dist + REVERSE_OVERSHOOT_DIST
    };

    if (current_path_dest == train->plan.dest.node) {
      current_path_dest_pos.offset = train->plan.dest.offset;
    }

    int dist_to_current_dest =
        get_distance_between(path, &train->current_pos, &current_path_dest_pos);

    switch (train->state) {
      case WAITING_FOR_INITIAL_POS:
        break;
      case SHORT_MOVE:
        // check if it's time to stop to reach destination
        if (train->move_stop_time >= Time(clock_server)) {
          TrainSetSpeed(train_tid, train->train, 0);
          train->state = DECELERATING;
        }

        break;
      case ACCELERATING:
        // TODO: measure time to accelerate then set to constant velocity to get accurate location
        // of train. for now we set to constant velocity immediately.
        train->state = CONSTANT_VELOCITY;
        break;
      case CONSTANT_VELOCITY:
        // check if it's time to stop to reach destination
        if (train->move_stop_time <= Time(clock_server)) {
          TrainSetSpeed(train_tid, train->train, 0);
          train->decel_begin_time = Time(clock_server);
          train->decel_duration = TRAINSET_DECEL_TIMES[train->train_index][train->speed];

          train->state = DECELERATING;
        }

        break;
      case DECELERATING:
        if (train->decel_begin_time + train->decel_duration >= Time(clock_server)) {
          train->state = STOPPED;
        }
        break;
      case PLAN_BEGIN:
        // check if first move is a reverse
        if (train->plan.paths[train->path_index].reverse) {
          TrainReverse(train_tid, train->train);
          train->state = STOPPED;
          break;
        }

        // short move (possibly add a buffer additionally?)
        if (dist_to_current_dest < TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed] +
                                       TRAINSET_ACCEL_DISTANCES[train->train_index][train->speed]) {
          train->move_start_time = Time(clock_server);
          train->move_duration =
              get_shortmove_duration(train->train, train->speed, dist_to_current_dest);
          train->move_stop_time = train->move_start_time + train->move_duration;
          train->state = SHORT_MOVE;

          TrainSetSpeed(train_tid, train->train, train->speed);
          TerminalLogPrint(
              terminal,
              "Train %d, performing a short move of %dmm for %d ticks",
              train->train,
              dist_to_current_dest,
              train->move_duration
          );
        } else {
          train->move_start_time = Time(clock_server);
          train->move_duration = get_move_time(train, dist_to_current_dest);
          train->move_stop_time = train->move_start_time + train->move_duration;
          train->state = ACCELERATING;

          TrainSetSpeed(train_tid, train->train, train->speed);
          TerminalLogPrint(
              terminal,
              "Train %d, performing a max velocity move of %dmm for %d ticks",
              train->train,
              dist_to_current_dest,
              train->move_duration
          );
          TerminalLogPrint(terminal, "to %s", current_path_dest->name);
        }
        break;
      case STOPPED:
        // when the train is stopped there are 2 things that could be true:
        // 1. the train has reached it's final destination.
        // 2. the train has reached the end of the simple path that it's currently on.

        // check if we're on the last simple path in path and the next node is our final destination
        // if so, we're done.
        if (train->path_index == train->plan.paths_len - 1 &&
            train->path_node_index == train->plan.path.nodes_len - 1) {
          route_train_randomly(terminal, train_planner, train);
          train->last_sensor_index = train->plan.path.nodes_len - 1;
          break;
        }

        ++train->path_index;

        // update current path
        current_path = train->plan.paths[train->path_index];
        current_path_dest = train->plan.path.nodes[current_path.end_index];
        current_path_dest_dir = train->plan.path.directions[current_path.end_index];
        current_path_dest_pos.node = current_path_dest;
        current_path_dest_pos.offset = current_path_dest->edge[current_path_dest_dir].dist;

        // check if move is a reverse
        if (train->plan.paths[train->path_index].reverse) {
          TrainReverse(train_tid, train->train);
          train->state = STOPPED;
          break;
        }

        // short move (possibly add a buffer additionally?)
        if (dist_to_current_dest < TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed] +
                                       TRAINSET_ACCEL_DISTANCES[train->train_index][train->speed]) {
          train->move_start_time = Time(clock_server);
          train->move_duration =
              get_shortmove_duration(train->train, train->speed, dist_to_current_dest);
          train->move_stop_time = train->move_start_time + train->move_duration;
          train->state = SHORT_MOVE;

          TrainSetSpeed(train_tid, train->train, train->speed);
          TerminalLogPrint(
              terminal,
              "Train %d, performing a short move of %dmm for %d ticks",
              train->train,
              dist_to_current_dest,
              train->move_duration
          );
        } else {
          train->move_start_time = Time(clock_server);
          train->move_duration = get_move_time(train, dist_to_current_dest);
          train->move_stop_time = train->move_start_time + train->move_duration;
          train->state = ACCELERATING;

          TrainSetSpeed(train_tid, train->train, train->speed);
          TerminalLogPrint(
              terminal,
              "Train %d, performing a max velocity move of %dmm for %d ticks",
              train->train,
              dist_to_current_dest,
              train->move_duration
          );
        }
        break;
    }
  }
}

// TrackNode indices of sensors indexed by train index
// designated sensors that trains should start at.
// train 1 - A1
// train 2 - A13
// train 24 - A15
// train 47 - A11
// train 54 - B7
// train 58 - B11
// train 77 - B9
// train 78 - C3
int initial_sensors[] = {0, 12, 14, 10, 22, 26, 24, 34};
int active_trains[] = {58};
int train_speeds[] = {10, 10, 10, 10, 10, 10, 10, 10};

// get train associated with sensor trigger. null if spurious.
struct Train *sensor_get_train(struct Train *train_states, int sensor) {
  // check if previous train location update was this sensor.
  // if so, return null because it means the train is sitting on top of it still since the last
  // sensor trigger.
  // check initial sensors to see if they're for one of the trains that are inactive.

  return NULL;
}

void set_train_active(struct Train *train, int speed) {
  train->active = true;
  train->speed = speed;
}

void train_manager_tick_notifier() {
  int train_manager = MyParentTid();
  int clock_server = WhoIs("clock_server");

  struct TrainManagerRequest req = {.type = TRAIN_MANAGER_TICK};

  while (true) {
    Delay(clock_server, 1);
    Send(train_manager, (const char *) &req, sizeof(req), NULL, 0);
  }
}

static void handle_update_sensors_request(
    int terminal,
    int train_tid,
    int train_planner,
    int clock_server,
    struct Train *train_states,
    struct TrainManagerUpdateSensorsRequest *req
) {
  int time = Time(clock_server);

  for (int sensor = 0; sensor < TRAINSET_SENSORS_LEN; ++sensor) {
    if (!req->sensors[sensor]) {
      continue;
    }

    struct Train *train = sensor_get_train(train, sensor);
    if (train == NULL) {
      continue;
    }

    struct TrackNode *sensor_node = &track[sensor];
    // update train position
    train->last_known_pos.node = sensor_node;
    train->last_known_pos.offset = 0;
    train->last_known_pos_time = time;

    train->current_pos.node = sensor_node;
    train->current_pos.offset = 0;
    train->current_pos_update_time = time;

    TerminalLogPrint(
        terminal, "Sensor hit %s attributed to Train %d.", sensor_node->name, train->train
    );

    if (!train->active) {
      set_train_active(train, train_speeds[train->train_index]);
      route_train_randomly(terminal, train_planner, train);
      train->last_sensor_index = train->plan.path.nodes_len - 1;
      return;
    }

    int next_sensor_index = get_next_sensor_index(train);
    if (next_sensor_index != -1) {
      train->last_sensor_index = next_sensor_index;
    }

    if (train->state == CONSTANT_VELOCITY) {
      struct SimplePath *cur_path = &train->plan.paths[train->path_index];
      struct TrackNode *current_path_dest = train->plan.path.nodes[cur_path->end_index];
      int current_path_dest_dir = train->plan.path.directions[cur_path->end_index];
      struct TrackPosition current_path_dest_pos = {
          .node = current_path_dest, .offset = current_path_dest->edge[current_path_dest_dir].dist
      };

      if (current_path_dest == train->plan.dest.node) {
        current_path_dest_pos.offset = train->plan.dest.offset;
      }

      train->move_stop_time = get_constant_velocity_travel_time_between(
          train, &train->current_pos, &current_path_dest_pos
      );
    }
  }
}

// TODO: need time/distance to accelerate to constant velocity.
// if the distance to the point we want to stop at is less than distance to constant velocity,
// we will do a short move instead.
void train_manager_task() {
  RegisterAs("train_manager");

  int clock_server = WhoIs("clock_server");
  int train_tid = WhoIs("train");
  int train_planner = WhoIs("train_planner");
  int terminal = WhoIs("terminal");

  struct Train trains[TRAINSET_NUM_TRAINS];

  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    int train_num = TRAINSET_TRAINS[i];
    train_init(&trains[i], train_num);
  }

  int tid;
  struct TrainManagerRequest req;

  // struct TrackPosition src = {.node = track[45].reverse, .offset = 0};
  // struct TrackPosition dest = {.node = &track[43], .offset = 0};

  // TerminalLogPrint(terminal, "Finding shortest path....");
  // TerminalLogPrint(terminal, "Creating route plan:");

  // struct RoutePlan route = CreatePlan(train_planner, &src, &dest);

  // TerminalLogPrint(
  //     terminal, "actual destination name: %s offset: %d", route.dest.node->name,
  //     route.dest.offset
  // );

  // if (route.path_found) {
  //   TerminalLogPrint(terminal, "Path found!");
  // } else {
  //   TerminalLogPrint(terminal, "Path not found!");
  // }

  // for (unsigned int i = 0; i < route.paths_len; ++i) {
  //   struct SimplePath path = route.paths[i];
  //   TerminalLogPrint(
  //       terminal,
  //       "start: %d, end: %d, reverse: %d\r\n",
  //       path.start_index,
  //       path.end_index,
  //       path.reverse
  //   );

  //   for (int j = path.start_index; j >= path.end_index; --j) {
  //     switch (route.path.directions[j]) {
  //       case DIR_AHEAD:
  //         TerminalLogPrint(
  //             terminal,
  //             "%s distance: %d, AHEAD",
  //             route.path.nodes[j]->name,
  //             route.path.nodes[j]->edge[DIR_AHEAD].dist
  //         );
  //         break;
  //       case DIR_CURVED:
  //         TerminalLogPrint(
  //             terminal,
  //             "%s distance: %d, CURVED",
  //             route.path.nodes[j]->name,
  //             route.path.nodes[j]->edge[DIR_CURVED].dist
  //         );
  //         break;
  //       case DIR_REVERSE:
  //         TerminalLogPrint(terminal, "%s, REVERSE", route.path.nodes[j]->name);
  //         break;
  //     }
  //   }
  // }

  // struct TrackPosition train_pos = {
  //     .node = route.path.nodes[route.paths[route.paths_len - 1].end_index], .offset = 0
  // };
  // struct TrackPosition dest_pos = {
  //     .node = route.path.nodes[route.paths[route.paths_len - 1].end_index], .offset = 0
  // };

  // struct Train *train = &trains[trainset_get_train_index(58)];
  // train_init(train, 58);

  // train_log(terminal, train);

  // int sensor_index = initial_sensors[train->train_index];
  // struct TrackNode *sensor_node = &track[sensor_index];
  // int time = Time(clock_server);

  // train->last_sensor_index = 0;
  // train->last_known_pos.node = sensor_node;
  // train->last_known_pos.offset = 0;
  // train->last_known_pos_time = time;

  // train->current_pos.node = sensor_node;
  // train->current_pos.offset = 0;
  // train->current_pos_update_time = time;

  // set_train_active(train, train_speeds[train->train_index]);
  // route_train_randomly(terminal, train_planner, train);

  // train->last_sensor_index = train->plan.path.nodes_len - 1;
  // train_log(terminal, train);

  // if (train->plan.path_found) {
  //   TerminalLogPrint(terminal, "Path found!");
  // } else {
  //   TerminalLogPrint(terminal, "Path not found!");
  // }

  // for (unsigned int i = 0; i < train->plan.paths_len; ++i) {
  //   struct SimplePath path = train->plan.paths[i];
  //   TerminalLogPrint(
  //       terminal,
  //       "start: %d, end: %d, reverse: %d\r\n",
  //       path.start_index,
  //       path.end_index,
  //       path.reverse
  //   );

  //   for (int j = path.start_index; j >= path.end_index; --j) {
  //     switch (train->plan.path.directions[j]) {
  //       case DIR_AHEAD:
  //         TerminalLogPrint(
  //             terminal,
  //             "%s distance: %d, AHEAD",
  //             train->plan.path.nodes[j]->name,
  //             train->plan.path.nodes[j]->edge[DIR_AHEAD].dist
  //         );
  //         break;
  //       case DIR_CURVED:
  //         TerminalLogPrint(
  //             terminal,
  //             "%s distance: %d, CURVED",
  //             train->plan.path.nodes[j]->name,
  //             train->plan.path.nodes[j]->edge[DIR_CURVED].dist
  //         );
  //         break;
  //       case DIR_REVERSE:
  //         TerminalLogPrint(terminal, "%s, REVERSE", train->plan.path.nodes[j]->name);
  //         break;
  //     }
  //   }
  // }

  // int dist_to_current_dest =
  //     get_distance_between(&train->plan.path, &train->plan.src, &train->plan.dest);
  // TerminalLogPrint(
  //     terminal,
  //     "distance between %s, offset: %d and %s offset: %d is %d",
  //     train->plan.src.node->name,
  //     train->plan.src.offset,
  //     train->plan.dest.node->name,
  //     train->plan.dest.offset,
  //     dist_to_current_dest
  // );

  // handle_tick(terminal, train_tid, train_planner, clock_server, trains);

  // TODO when we reverse, flip current_pos.
  Create(TRAIN_TASK_PRIORITY, train_manager_tick_notifier);

  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case TRAIN_MANAGER_UPDATE_SENSORS:
        handle_update_sensors_request(
            terminal, train_tid, train_planner, clock_server, trains, &req.update_sens_req
        );
        Reply(tid, NULL, 0);
        break;
      case TRAIN_MANAGER_TICK:
        handle_tick(terminal, train_tid, train_planner, clock_server, trains);
        Reply(tid, NULL, 0);
    }
  }
}

void NewTrain(int train) {}

// Executes the plan for the provided train and stops execution of
// any currently running plans
void ExecutePlan(struct RoutePlan plan) {}

void TrainManagerUpdateSensors(int tid, bool *sensors) {
  struct TrainManagerRequest req = {
      .type = TRAIN_MANAGER_UPDATE_SENSORS, .update_sens_req = {.sensors = sensors}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

// LEFT TO DO:
// - reservations as we move
// - check for reservations in our path.
