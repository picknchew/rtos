#include "train_manager.h"

#include <stdbool.h>

#include "irq.h"
#include "selected_track.h"
#include "syscall.h"
#include "task.h"
#include "track_position.h"
#include "track_reservations.h"
#include "train_planner.h"
#include "train_sensor_notifier.h"
#include "trainset.h"
#include "trainset_calib_data.h"
#include "trainset_task.h"
#include "user/server/clock_server.h"
#include "user/server/name_server.h"
#include "user/terminal/terminal_task.h"
#include "util.h"

// m: initial zone res

// 21cm
static const int TRAIN_LEN = 210;
// offset of the front of the traint from the sensor
// 14cm
static const int TRAIN_FORWARD_FRONT_OFFSET = 140;
// 2cm
static const int TRAIN_REVERSE_FRONT_OFFSET = 20;

enum TrainState {
  PATH_BEGIN,
  SHORT_MOVE,
  ACCELERATING,
  CONSTANT_VELOCITY,
  DECELERATING,
  SHORT_MOVE_DECELERATING,
  STOPPED,
  WAITING_FOR_INITIAL_POS,
  LOCKED
};

const char *train_state_to_string(enum TrainState state) {
  switch (state) {
    case PATH_BEGIN:
      return "PATH_BEGIN";
    case SHORT_MOVE:
      return "SHORT_MOVE";
    case ACCELERATING:
      return "ACCELERATING";
    case CONSTANT_VELOCITY:
      return "CONSTANT_VELOCITY";
    case DECELERATING:
      return "DECELERATING";
    case SHORT_MOVE_DECELERATING:
      return "SHORT_MOVE_DECELERATING";
    case STOPPED:
      return "STOPPED";
    case WAITING_FOR_INITIAL_POS:
      return "WAITING_FOR_INITIAL_POS";
    case LOCKED:
      return "LOCKED";
  }

  return "UNKNOWN";
}

struct ShortMove {
  int start_time;
  int duration;
};

enum PathFindState {
  RAND_ROUTE,
  RETURN_HOME,
  ROUTE_TO_SELECTED_DEST,
  NONE,
};

// ticks to wait after a short move.
static int SHORT_MOVE_DELAY = 400;

struct Train {
  bool active;

  enum PathFindState pf_state;
  struct TrackNode *selected_dest;

  struct TrackNode *initial_pos;

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
  unsigned int path_index;

  struct TrainPosition est_pos;
  int est_pos_update_time;
  struct TrainPosition last_known_pos;
  int last_known_pos_time;

  int speed;

  struct TrackNode *next_sensor;
  int next_sensor_eta;
  int sensor_eta_error;

  int terminal_update_time;
  // used for positioning the front of the train
  bool reversed;

  FixedPointInt acceleration;
  FixedPointInt velocity;
  int lock_begin_time;
  char train_color;
};

static int get_distance_between_debug(
    struct Path *path,
    struct TrackPosition *src,
    struct TrackPosition *dest,
    bool strict
) {
  int dist = -src->offset + dest->offset;

  TerminalLogPrint(WhoIs("terminal"), "initial dist %d", dist);

  int pos1_index = -1;
  // find pos1 index in path
  for (int i = 0; i < path->nodes_len; ++i) {
    if (src->node == path->nodes[i] || (!strict && src->node == path->nodes[i]->reverse)) {
      pos1_index = i;
      break;
    }
  }

  if (pos1_index == -1) {
    TerminalLogPrint(WhoIs("terminal"), "couldn't find node %s in train path!", src->node->name);
    while (true) {}
    return -1;
  }

  TerminalLogPrint(WhoIs("terminal"), "found initial node index %d", pos1_index);

  // find distance from pos1 to pos2
  for (int i = pos1_index; i < path->nodes_len; ++i) {
    struct TrackNode *node = path->nodes[i];
    int edge_dir = path->directions[i];

    if (node == dest->node || (!strict && node == dest->node->reverse)) {
      break;
    }

    if (edge_dir == DIR_REVERSE) {
      continue;
    }

    TerminalLogPrint(WhoIs("terminal"), "nodes between are %s", node->name);

    dist += node->edge[edge_dir].dist;
  }

  TerminalLogPrint(WhoIs("terminal"), "final dist %d", dist);

  return dist;
}

// TODO: if train hits any sensor after a node in the path, this won't work because it cannot find
// the destinaton. theoretically, this should never happen because the train should always stop at
// the destination.
// if strict, nodes must match src and dest and not their reverse nodes.
static int get_distance_between(
    struct Path *path,
    struct TrackPosition *src,
    struct TrackPosition *dest,
    bool strict
) {
  // TODO: incorporate simple path destination offsets to get the correct distances in between
  // simple paths
  int dist = -src->offset + dest->offset;

  int pos1_index = -1;
  // find pos1 index in path
  for (int i = 0; i < path->nodes_len; ++i) {
    if (src->node == path->nodes[i] || (!strict && src->node == path->nodes[i]->reverse)) {
      pos1_index = i;
      break;
    }
  }

  if (pos1_index == -1) {
    TerminalLogPrint(WhoIs("terminal"), "couldn't find node %s in train path!", src->node->name);
    while (true) {}
    return -1;
  }

  // find distance from pos1 to pos2
  for (int i = pos1_index; i < path->nodes_len; ++i) {
    struct TrackNode *node = path->nodes[i];
    int edge_dir = path->directions[i];

    if (node == dest->node || (!strict && node == dest->node->reverse)) {
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
  TerminalLogPrint(terminal, "train: %d", train->active);

  switch (train->state) {
    case PATH_BEGIN:
      TerminalLogPrint(terminal, "state: PATH_BEGIN");
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
    case SHORT_MOVE_DECELERATING:
      TerminalLogPrint(terminal, "state: SHORT_MOVE_DECELERATING");
      break;
    case STOPPED:
      TerminalLogPrint(terminal, "state: STOPPED");
      break;
    case WAITING_FOR_INITIAL_POS:
      TerminalLogPrint(terminal, "state: WAITING_FOR_INITIAL_POS");
      break;
    case LOCKED:
      TerminalLogPrint(terminal, "state; LOCKED");
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
        "est_pos: %s, offset: %d",
        train->est_pos.position.node->name,
        train->est_pos.position.offset
    );
    TerminalLogPrint(terminal, "est_pos_update_time: %d", train->est_pos_update_time);
  }
}

static void train_update_terminal(int terminal, struct Train *train, int time) {
  if (time - train->terminal_update_time < 10) {
    return;
  }

  train->terminal_update_time = time;

  const char *pos_node = "N/A";
  int pos_offset = 0;
  const char *state = train_state_to_string(train->state);
  const char *next_sensor = train->next_sensor != NULL ? train->next_sensor->name : "N/A";
  const char *dest = "N/A";

  if (train->state != WAITING_FOR_INITIAL_POS) {
    pos_node = train->est_pos.position.node->name;
    pos_offset = train->est_pos.position.offset;

    if (train->plan.path_found) {
      dest = train->plan.dest.node->name;
    }
  }

  TerminalUpdateTrainInfo(
      terminal,
      train->train,
      pos_node,
      pos_offset,
      state,
      next_sensor,
      train->next_sensor_eta,
      train->sensor_eta_error,
      dest,
      train->velocity,
      train->acceleration
  );
}

void train_init(struct Train *train, int train_num) {
  train->active = false;

  train->pf_state = NONE;
  train->initial_pos = NULL;
  train->selected_dest = NULL;

  train->train = train_num;
  train->train_index = trainset_get_train_index(train_num);

  train->state = WAITING_FOR_INITIAL_POS;
  train->move_start_time = 0;
  train->move_duration = 0;
  train->move_stop_time = 0;

  train->last_sensor_index = 0;
  train->last_switch_index = -1;

  train->plan.path_found = false;
  train->path_index = 0;

  train->est_pos_update_time = 0;
  train->last_known_pos_time = 0;

  train->next_sensor = NULL;
  train->next_sensor_eta = 0;
  train->sensor_eta_error = 0;

  train->reversed = false;
  train->speed = 0;
  train->acceleration = 0;
  train->velocity = 0;

  train->terminal_update_time = -100;

  train->lock_begin_time = 0;
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
  int dist = get_distance_between(&train->plan.path, pos1, pos2, true);

  return get_constant_velocity_travel_time(train, dist);
}

enum TrainManagerRequestType {
  TRAIN_MANAGER_UPDATE_SENSORS,
  TRAIN_MANAGER_TICK,
  TRAIN_MANAGER_ROUTE_RETURN,
  TRAIN_MANAGER_RAND_ROUTE
};

struct TrainManagerUpdateSensorsRequest {
  bool *sensors;
};

struct TrainManagerRouteReturnRequest {
  int train1;
  int train2;
  char *dest1;
  char *dest2;
};

struct TrainManagerRandRouteRequest {
  int train1;
  int train2;
};

struct TrainManagerRequest {
  enum TrainManagerRequestType type;

  union {
    struct TrainManagerUpdateSensorsRequest update_sens_req;
    struct TrainManagerRouteReturnRequest route_return_req;
    struct TrainManagerRandRouteRequest rand_route_req;
  };
};

inline static FixedPointInt
calculate_vd_accel(FixedPointInt initial_velocity, FixedPointInt final_velocity, int dist) {
  // need to unwrap once to get FixedPointInt since we multiply two fixed point ints together
  return fixed_point_int_get(
      (final_velocity * final_velocity - initial_velocity * initial_velocity) / (2 * dist)
  );
}

inline static FixedPointInt
calculate_vt_accel(FixedPointInt initial_velocity, FixedPointInt final_velocity, int time) {
  return (final_velocity - initial_velocity) / time;
}

inline static FixedPointInt calculate_dt_accel(int dist, int time) {
  return fixed_point_int_from(2 * dist) / (time * time);
}

inline static int calculate_decel_time(FixedPointInt initial_velocity, FixedPointInt decel) {
  return -initial_velocity / decel;
}

inline static FixedPointInt get_train_accel(struct Train *train) {
  return calculate_dt_accel(
      TRAINSET_ACCEL_DISTANCES[train->train_index][train->speed],
      TRAINSET_ACCEL_TIMES[train->train_index][train->speed]
  );
}

inline static FixedPointInt get_train_decel(struct Train *train) {
  // assuming constant deceleration
  return calculate_vd_accel(
      TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed],
      0,
      TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed]
  );
}

inline int get_dist_travelled(FixedPointInt initial_velocity, FixedPointInt accel, int time) {
  // d = v0 * t + 1/2 * at^2
  return fixed_point_int_get(initial_velocity * time + (accel * time * time) / 2);
}

inline int get_dist_constant_velocity(FixedPointInt velocity, int time) {
  return fixed_point_int_get(velocity * time);
}

int calculate_travel_time(FixedPointInt accel, FixedPointInt initial_velocity, int dist) {
  if (accel == 0) {
    // assume constant velocity.
    return fixed_point_int_from(dist) / initial_velocity;
  }

  // dist = initial_velocity * t + 1/2 * accel * time^2
  // 0 = -dist + initial_velocity * t + 1/2 * accel * time^2
  FixedPointInt a = accel / 2;
  FixedPointInt b = initial_velocity;
  FixedPointInt c = fixed_point_int_from(-dist);

  // sqrt(4 * 2 * 100)

  int64_t discriminant = b * b - 4L * a * c;

  TerminalLogPrint(WhoIs("terminal"), "discriminant %d", discriminant);
  // TerminalLogPrint(WhoIs("terminal"), "discriminant sqrt %d", sqrt(discriminant));
  TerminalLogPrint(WhoIs("temrinal"), "a %d", a);
  TerminalLogPrint(WhoIs("terminal"), "b %d", b);
  TerminalLogPrint(WhoIs("terminal"), "b^2 %d", b * b);
  TerminalLogPrint(WhoIs("terminal"), "4ac %d", 4 * a * c);
  TerminalLogPrint(WhoIs("terminal"), "dist %d", c);

  if (discriminant == 0) {
    return -b / (2 * a);
  }

  // discriminant > 0
  int64_t t1 = (-b - sqrt(discriminant)) / (2 * a);
  int64_t t2 = (-b + sqrt(discriminant)) / (2 * a);

  return t1 > t2 ? t1 : t2;
}

static int train_get_travel_time(struct Train *train, int dist, int current_time) {
  int est_time = 0;

  FixedPointInt accel = train->acceleration;
  FixedPointInt velocity = train->velocity;

  switch (train->state) {
    case ACCELERATING: {
      int accel_end_time =
          train->move_start_time + TRAINSET_ACCEL_TIMES[train->train_index][train->speed];
      int accel_dist = get_dist_travelled(velocity, accel, accel_end_time - current_time);

      // check if dist we need to travel is less than or equal to the distance required to
      // accelerate to max velocity.
      if (dist <= accel_dist) {
        est_time = calculate_travel_time(accel, train->velocity, dist);
        break;
      }

      // need to include constant velocity portion to cover distance.
      dist -= accel_dist;
      est_time += calculate_travel_time(accel, velocity, accel_dist);

      accel = 0;
      velocity = TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed];
    }
      // fall through
    case CONSTANT_VELOCITY: {
      int constant_velocity_dist =
          get_dist_constant_velocity(velocity, train->move_stop_time - current_time);

      if (dist <= constant_velocity_dist) {
        est_time += calculate_travel_time(accel, velocity, dist);
        break;
      }

      dist -= constant_velocity_dist;
      est_time += calculate_travel_time(accel, velocity, constant_velocity_dist);

      accel = get_train_decel(train);
    }
      // fall through
    case DECELERATING:
      TerminalLogPrint(WhoIs("terminal"), "before calculate_travel tiem");
      est_time += calculate_travel_time(accel, velocity, dist);
      break;

    case SHORT_MOVE:
    case SHORT_MOVE_DECELERATING:
      est_time += calculate_travel_time(accel, velocity, dist);
      break;
    default:
      break;
  }

  return est_time;
}

// returns time to move distance for a full acceleration to constant velocity move
int get_move_time(struct Train *train, int distance) {
  distance -= TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed];

  int constant_velocity_time = get_constant_velocity_travel_time(
      train, (distance - TRAINSET_ACCEL_DISTANCES[train->train_index][train->speed])
  );

  return TRAINSET_ACCEL_TIMES[train->train_index][train->speed] + constant_velocity_time;
}

int shortmove_get_velocity(struct Train *train, int dist) {
  // assuming constant velocity, add to account for deceleration time
  int time_delta = shortmove_get_duration(train->train, train->speed, dist) + SHORT_MOVE_DELAY;
  return fixed_point_int_from(dist) / time_delta;
}

void update_train_pos(int time, struct Train *train) {
  if (train->state == STOPPED || train->state == WAITING_FOR_INITIAL_POS ||
      train->state == PATH_BEGIN) {
    return;
  }

  struct SimplePath current_path = train->plan.paths[train->path_index];
  // destination node of simple path
  struct TrackNode *current_path_dest = current_path.dest.node;
  int current_path_dest_dir = current_path.dest_dir;
  struct TrackPosition current_path_dest_pos = {
      .node = current_path.dest.node, .offset = current_path.dest.offset
      // .offset = current_path_dest->edge[current_path_dest_dir].dist + REVERSE_OVERSHOOT_DIST
  };

  if (current_path_dest == train->plan.dest.node) {
    current_path_dest_pos.offset = train->plan.dest.offset;
  }

  switch (train->state) {
    case SHORT_MOVE:
      if (train->move_stop_time <= time) {
        train->est_pos = train_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(
                train->velocity, 0, train->move_stop_time - train->est_pos_update_time
            )
        );
        train->est_pos_update_time = train->move_stop_time;
      } else {
        // assume constant velocity for short moves
        train->est_pos = train_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(train->velocity, 0, time - train->est_pos_update_time)
        );
        train->est_pos_update_time = time;
      }

      break;
    case SHORT_MOVE_DECELERATING:
      // reached destination
      if (train->decel_begin_time + train->decel_duration <= time) {
        train->est_pos.position = current_path_dest_pos;
        train->est_pos.last_dir = current_path_dest_dir;
        train->est_pos_update_time = time;

        int decel_end_time = train->decel_begin_time + train->decel_duration;

        train->est_pos = train_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(train->velocity, 0, decel_end_time - train->est_pos_update_time)
        );
        train->est_pos_update_time = decel_end_time;
      } else {
        // TerminalLogPrint(terminal, "short move const velocity %d, time diff: %d, dist: %d",
        // train->velocity, time - train->est_pos_update_time, get_dist_travelled(train->velocity,
        // 0, time - train->est_pos_update_time)); constant velocity
        train->est_pos = train_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(train->velocity, 0, time - train->est_pos_update_time)
        );
        train->est_pos_update_time = time;
      }

      break;
    case ACCELERATING:
      if (train->move_start_time + TRAINSET_ACCEL_TIMES[train->train_index][train->speed] <= time) {
        int accel_end_time =
            train->move_start_time + TRAINSET_ACCEL_TIMES[train->train_index][train->speed];

        train->est_pos = train_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(
                train->velocity, train->acceleration, accel_end_time - train->est_pos_update_time
            )
        );
        // we don't set velocity here because when state changes it gets set to the train's max
        // velocity.
        train->est_pos_update_time = accel_end_time;
      } else {
        train->est_pos = train_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(
                train->velocity, train->acceleration, time - train->est_pos_update_time
            )
        );

        train->velocity += train->acceleration * (time - train->est_pos_update_time);
        train->est_pos_update_time = time;
      }

      // TerminalLogPrint(
      //       terminal,
      //       "ACCELERATING start time %d duration: %d",
      //       train->move_start_time,
      //       TRAINSET_ACCEL_TIMES[train->train_index][train->speed]
      //   );

      // TerminalLogPrint(terminal, "velocity sum %d", train->acceleration * (time -
      // train->est_pos_update_time)); TerminalLogPrint(terminal, "acceleration %d, time diff %d",
      // train->acceleration, time - train->est_pos_update_time); TerminalLogPrint(terminal, "new
      // velocity %d", train->velocity);

      break;
    case CONSTANT_VELOCITY:
      if (train->move_stop_time <= time) {
        train->est_pos = train_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(
                train->velocity, 0, train->move_stop_time - train->est_pos_update_time
            )
        );
        train->est_pos_update_time = train->move_stop_time;
      } else {
        train->est_pos = train_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(train->velocity, 0, time - train->est_pos_update_time)
        );
        train->est_pos_update_time = time;
      }

      break;
    case DECELERATING:
      if (train->decel_begin_time + train->decel_duration <= time) {
        int decel_end_time = train->decel_begin_time + train->decel_duration;

        train->est_pos.position = current_path_dest_pos;
        train->est_pos.last_dir = current_path_dest_dir;

        // train->est_pos = train_position_add(
        //     train->est_pos,
        //     &train->plan.path,
        //     get_dist_travelled(train->velocity, train->accelerdation, decel_end_time -
        //     train->est_pos_update_time)
        // );
        train->est_pos_update_time = decel_end_time;
        // we don't set velocity here because when state changes it gets set to 0.
      } else {
        train->est_pos = train_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(train->velocity, 0, time - train->est_pos_update_time)
        );

        train->velocity += train->acceleration * (time - train->est_pos_update_time);
        // velocity should always be positive
        train->velocity = max(train->velocity, 0);
        train->est_pos_update_time = time;
      }
      break;
    default:
      break;
  }
}

static int get_next_sensor_index(struct Train *train) {
  for (int i = train->last_sensor_index + 1; i < train->plan.path.nodes_len; ++i) {
    struct TrackNode *node = train->plan.path.nodes[i];

    if (node->type == NODE_SENSOR) {
      // if we're not on the last path..
      if (train->path_index != train->plan.paths_len - 1) {
        for (unsigned int j = train->path_index; j < train->plan.paths_len; ++j) {
          if (train->plan.paths[j].end_index > i) {
            return i;
          }

          // we skip the last sensors for intermediate paths since they're not actually reached by
          // the train
          if (train->plan.paths[j].end_index == i) {
            break;
          }
        }
      } else {
        return i;
      }
    }
  }

  return -1;
}

static int get_next_switch_index(struct Train *train) {
  for (int i = train->last_switch_index + 1; i < train->plan.path.nodes_len; ++i) {
    struct TrackNode *node = train->plan.path.nodes[i];

    if (node->type == NODE_BRANCH) {
      return i;
    }

    // TODO: maybe skip branch if it's the last node of an intermediate path since we don't actually
    // go through the switch.
  }

  return -1;
}

static void train_update_next_sensor(struct Train *train, int current_time) {
  int terminal = WhoIs("terminal");

  // lookahead one sensor
  int next_sensor_index = get_next_sensor_index(train);
  if (next_sensor_index != -1) {
    train->next_sensor = train->plan.path.nodes[next_sensor_index];

    TerminalLogPrint(terminal, "next sensor is %s", train->next_sensor->name);

    // check if sensor is currently in the current simple path.
    struct SimplePath *cur_path = &train->plan.paths[train->path_index];

    TerminalLogPrint(
        terminal,
        "current path range %d -- %d, next sensor index: %d, next sensor: %s",
        cur_path->start_index,
        cur_path->end_index,
        next_sensor_index,
        train->next_sensor->name
    );

    if (next_sensor_index < cur_path->start_index || next_sensor_index > cur_path->end_index) {
      // if it isn't in the current path, we can't provide an estimate yet
      train->next_sensor_eta = 0;
      return;
    }

    // struct TrackPosition next_sensor_pos = {.node = train->next_sensor, .offset = 0};

    // TerminalLogPrint(terminal, "before dist between");
    // int dist_to_sensor =
    //     get_distance_between(&train->plan.path, &train->est_pos.position, &next_sensor_pos,
    //     true);

    // TerminalLogPrint(terminal, "after dist between");
    // if (dist_to_sensor < 0) {
    //   TerminalLogPrint(terminal, "error! distance to next sensor is negative!");
    //   train->next_sensor_eta = 0;
    //   return;
    // }
    // TerminalLogPrint(terminal, "before travel time");
    // train->next_sensor_eta =
    //     current_time + train_get_travel_time(train, dist_to_sensor, current_time);
    // TerminalLogPrint(terminal, "after travel time");
  } else {
    train->next_sensor = NULL;
    train->next_sensor_eta = 0;
  }
}

void reroute_train(int terminal, int train_planner, struct Train *train, int time) {
  if (train->pf_state == RAND_ROUTE) {
    struct TrackPosition rand_dest = track_position_random(terminal);
    train->plan = CreatePlan(train_planner, &train->est_pos, &rand_dest);

    while (!train->plan.path_found) {
      TerminalLogPrint(
          terminal,
          "No path to %s from %s, trying to find another destination.",
          rand_dest.node->name,
          train->est_pos.position.node->name
      );
      rand_dest = track_position_random(terminal);
      train->plan = CreatePlan(train_planner, &train->est_pos, &rand_dest);
    }
  } else if (train->pf_state == ROUTE_TO_SELECTED_DEST) {
    struct TrackPosition dest = {.node = train->selected_dest, .offset = 0};
    train->plan = CreatePlan(train_planner, &train->est_pos, &dest);

    if (!train->plan.path_found) {
      TerminalLogPrint(
          terminal,
          "Failed to find path to %s from %s!",
          dest.node->name,
          train->est_pos.position.node->name
      );
    }

    // next path find, the train will return home.
    train->pf_state = RETURN_HOME;
  } else {
    // pf_state == RETURN_HOME
    struct TrackPosition dest = {.node = train->initial_pos, .offset = 0};
    train->plan = CreatePlan(train_planner, &train->est_pos, &dest);

    if (!train->plan.path_found) {
      TerminalLogPrint(
          terminal,
          "Failed to find path to %s from %s!",
          dest.node->name,
          train->est_pos.position.node->name
      );
    }

    train->pf_state = NONE;
  }

  TerminalLogPrint(
      terminal,
      "\033[35mPath from %s to %s",
      train->plan.src.node->name,
      train->plan.dest.node->name
  );

  for (unsigned int i = 0; i < train->plan.paths_len; ++i) {
    struct SimplePath path = train->plan.paths[i];
    TerminalLogPrint(
        terminal,
        "start: %d, end: %d, reverse: %d, dest: %s +%d\r\n",
        path.start_index,
        path.end_index,
        path.reverse,
        path.dest.node->name,
        path.dest.offset
    );

    for (int j = path.start_index; j <= path.end_index; ++j) {
      switch (train->plan.path.directions[j]) {
        case DIR_AHEAD:
          TerminalLogPrint(
              terminal,
              "%s distance: %d, AHEAD",
              train->plan.path.nodes[j]->name,
              train->plan.path.nodes[j]->edge[DIR_AHEAD].dist
          );
          break;
        case DIR_CURVED:
          TerminalLogPrint(
              terminal,
              "%s distance: %d, CURVED",
              train->plan.path.nodes[j]->name,
              train->plan.path.nodes[j]->edge[DIR_CURVED].dist
          );
          break;
        case DIR_REVERSE:
          TerminalLogPrint(terminal, "%s, REVERSE", train->plan.path.nodes[j]->name);
          break;
      }
    }
  }

  train->path_index = 0;

  train->last_switch_index = -1;
  train->state = PATH_BEGIN;

  train->last_sensor_index = -1;

  train_update_next_sensor(train, time);

  // if the train is currently at the first sensor in the path.
  // position is known at this point
  if (train->next_sensor == train->est_pos.position.node ||
      train->next_sensor == train->est_pos.position.node->reverse) {
    train->last_sensor_index = get_next_sensor_index(train);
  }

  TerminalLogPrint(terminal, "Routing train %d to %s.", train->train, train->plan.dest.node->name);
}

static const int FLIP_SWITCH_DIST = 300;
static const int DEADLOCK_DURATION = 3000;
static const int WAIT_DURATION = 300;

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

    int time = Time(clock_server);

    train_update_terminal(terminal, train, time);
    update_train_pos(time, train);

    struct Path *path = &train->plan.path;

    // TODO:
    // - make sure we are within reservation range ^

    // when we are close enough to the next switch, activate it.
    int next_switch_index = get_next_switch_index(train);

    // // TerminalLogPrint(terminal, "next switch_index %d", next_switch_index);
    // // we check if next switch is equal to last switch in case we already activated
    // // the switch previously.
    if (next_switch_index != -1 && next_switch_index != train->last_switch_index) {
      struct TrackNode *next_switch = path->nodes[next_switch_index];
      struct TrackPosition next_switch_pos = {.node = next_switch, .offset = 0};

      // TerminalLogPrint(
      //     terminal,
      //     "\033[33mnext switch is %s %dmm/%dmm away",
      //     next_switch->name,
      //     get_distance_between(path, &train->est_pos, &next_switch_pos),
      //     FLIP_SWITCH_DIST
      // );
      // TerminalLogPrint(
      //     terminal,
      //     "distance to next switch is %d",
      //     get_distance_between(&train->plan.path, &train->est_pos, &next_switch_pos)
      // );

      // only flip switch if we've reserved the zone or we're close enough.
      if (ZoneOccupied(next_switch->zone) == train->train_index &&
          get_distance_between(path, &train->est_pos.position, &next_switch_pos, true) <=
              FLIP_SWITCH_DIST) {
        switch (path->directions[next_switch_index]) {
          case DIR_CURVED:
            TerminalLogPrint(
                terminal,
                "\033[33mset switch %d to CURVED, estimated dist to switch: %d",
                next_switch->num,
                get_distance_between(path, &train->est_pos.position, &next_switch_pos, true)
            );
            TrainSetSwitchDir(train_tid, next_switch->num, TRAINSET_DIRECTION_CURVED);
            break;
          case DIR_AHEAD:
            TerminalLogPrint(
                terminal,
                "\033[33mset switch %d to STRAIGHT estimated dist to switch: %d",
                next_switch->num,
                get_distance_between(path, &train->est_pos.position, &next_switch_pos, true)
            );
            TrainSetSwitchDir(train_tid, next_switch->num, TRAINSET_DIRECTION_STRAIGHT);
        }

        train->last_switch_index = next_switch_index;
      }
    }

    struct SimplePath current_path = train->plan.paths[train->path_index];
    // destination node of simple path
    struct TrackNode *current_path_dest = current_path.dest.node;
    // TerminalLogPrint(terminal, "current path dest");
    int current_path_dest_dir = current_path.dest_dir;
    // TerminalLogPrint(terminal, "current path dest dir");
    struct TrackPosition current_path_dest_pos = {
        .node = current_path.dest.node, .offset = current_path.dest.offset
        // .offset = current_path_dest->edge[current_path_dest_dir].dist + REVERSE_OVERSHOOT_DIST
    };

    if (current_path_dest == train->plan.dest.node) {
      current_path_dest_pos.offset = train->plan.dest.offset;
    }

    int dist_to_current_dest =
        get_distance_between(path, &train->est_pos.position, &current_path_dest_pos, true);
    switch (train->state) {
      case WAITING_FOR_INITIAL_POS:
        break;
      case SHORT_MOVE:
        // check if it's time to stop to reach destination
        if (train->move_stop_time <= time) {
          TrainSetSpeed(train_tid, train->train, 0);
          train->decel_begin_time = time;
          train->decel_duration = SHORT_MOVE_DELAY;
          TerminalLogPrint(terminal, "state change to SHORT_MOVE_DECELERATING");
          train->state = SHORT_MOVE_DECELERATING;
        }

        break;
      case ACCELERATING:
        if (train->move_start_time + TRAINSET_ACCEL_TIMES[train->train_index][train->speed] <=
            time) {
          train->state = CONSTANT_VELOCITY;
          // train reached max speed
          train->velocity = TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed];
          train->acceleration = 0;
        }
        break;
      case CONSTANT_VELOCITY:
        // check if it's time to stop to reach destination
        if (train->move_stop_time <= time) {
          TrainSetSpeed(train_tid, train->train, 0);
          train->decel_begin_time = time;
          train->decel_duration = TRAINSET_DECEL_TIMES[train->train_index][train->speed];
          train->acceleration = get_train_decel(train);

          TerminalLogPrint(terminal, "state change to DECELERATING");
          train->state = DECELERATING;
        }

        break;
      case SHORT_MOVE_DECELERATING:
        if (train->decel_begin_time + train->decel_duration <= time) {
          TerminalLogPrint(terminal, "state change to STOPPED from SHORT_MOVE_DECELERATING");

          train->velocity = 0;
          train->acceleration = 0;
          train->state = STOPPED;
        }
        break;
      case DECELERATING:
        if (train->decel_begin_time + train->decel_duration <= time) {
          TerminalLogPrint(terminal, "state change to STOPPED from DECELERATING");

          TerminalLogPrint(
              terminal,
              "decel_begin_time: %d, decel_duration: %d",
              train->decel_begin_time,
              train->decel_duration
          );

          train->velocity = 0;
          train->acceleration = 0;
          train->state = STOPPED;

          train_update_terminal(terminal, train, time);
        }
        break;
      case PATH_BEGIN:
        // check if move is a reverse
        if (current_path.reverse) {
          TrainReverseInstant(train_tid, train->train);
          train->reversed = !train->reversed;

          // reverse train direction and make the back of the train the front of the train.
          // TODO: when reversing our train pos, we always make the offset positive. as a result,
          // it's possible that the resulting node is not within our path. this makes all our
          // distance calculations return -1 since it cannot find the source node.
          // train->est_pos = train_position_reverse_node(train->est_pos, train_tid);
          // train->est_pos = train_position_add(train->est_pos, &train->plan.path, TRAIN_LEN);

          TerminalLogPrint(
              terminal,
              "train reversed: orig pos %s +%d",
              train->est_pos.position.node->name,
              train->est_pos.position.offset
          );
          train->est_pos.position.node = train->est_pos.position.node->reverse;
          train->est_pos.position.offset = -train->est_pos.position.offset + TRAIN_LEN;

          TerminalLogPrint(
              terminal,
              "train reversed: new pos %s +%d",
              train->est_pos.position.node->name,
              train->est_pos.position.offset
          );

          // train->est_pos.position.offset = 0;
          TerminalLogPrint(terminal, "state change to STOPPED from PATH_BEGIN");
          TerminalLogPrint(terminal, "Train %d reversing", train->train);
          train->state = STOPPED;
          break;
        }

        // short move (possibly add a buffer additionally?)
        if (dist_to_current_dest < TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed] +
                                       TRAINSET_ACCEL_DISTANCES[train->train_index][train->speed]) {
          // get_distance_between(path, &train->est_pos.position, &current_path_dest_pos, true);
          train->move_start_time = time;
          train->move_duration =
              shortmove_get_duration(train->train, train->speed, dist_to_current_dest);
          train->move_stop_time = train->move_start_time + train->move_duration;

          TerminalLogPrint(terminal, "state change to SHORT_MOVE from PATH_BEGIN");
          // TerminalLogPrint(terminal, "SHORT_MOVE reservation");

          if (ReservePath(
                  terminal,
                  &train->plan,
                  &train->plan.paths[train->path_index],
                  train->last_sensor_index + 1,
                  train->train_index
              )) {
            TerminalLogPrint(terminal, "SHORT_MOVE reservation successful");
          } else {
            TerminalLogPrint(terminal, "SHORT_MOVE reservation unsucessful, train is LOCKED");
            train->state = LOCKED;
            train->lock_begin_time = time;
            continue;
          }

          train->state = SHORT_MOVE;
          train->velocity = shortmove_get_velocity(train, dist_to_current_dest);
          train->acceleration = 0;

          TrainSetSpeed(train_tid, train->train, train->speed);
          TerminalLogPrint(
              terminal,
              "Train %d, performing a short move of %dmm to %s +%d for %d ticks from %s +%d",
              train->train,
              dist_to_current_dest,
              current_path_dest_pos.node->name,
              current_path_dest_pos.offset,
              train->move_duration,
              train->est_pos.position.node->name,
              train->est_pos.position.offset
          );
        } else {
          train->move_start_time = time;
          train->move_duration = get_move_time(train, dist_to_current_dest);
          train->move_stop_time = train->move_start_time + train->move_duration;
          TerminalLogPrint(terminal, "state change to ACCELERATING from PATH_BEGIN");

          struct TrackNode *dest = current_path_dest_pos.node;
          // TerminalLogPrint(terminal, "last node name %s",
          // plan.path.nodes[last_node_index]->name); TerminalLogPrint(
          //     terminal, "last node index -1  %s", plan.path.nodes[last_node_index - 1]->name
          // );

          if (ReservePath(
                  terminal,
                  &train->plan,
                  &train->plan.paths[train->path_index],
                  train->last_sensor_index + 1,
                  train->train_index
              )) {
            TerminalLogPrint(terminal, "ACCELERATION reservation sucessful");
          } else {
            TerminalLogPrint(terminal, "ACCELERATION reservation unsucessful, train is LOCKED");
            train->state = LOCKED;
            train->lock_begin_time = time;
            continue;
          }

          train->state = ACCELERATING;
          train->acceleration = get_train_accel(train);

          TrainSetSpeed(train_tid, train->train, train->speed);
          TerminalLogPrint(
              terminal,
              "Train %d, performing a max velocity move of %dmm to %s +%d for %d ticks from %s +%d",
              train->train,
              dist_to_current_dest,
              current_path_dest_pos.node->name,
              current_path_dest_pos.offset,
              train->move_duration,
              train->est_pos.position.node->name,
              train->est_pos.position.offset
          );

          // TerminalLogPrint(terminal, "get_distance_between_debug");
          // int dist_to_current_dest = get_distance_between_debug(
          //     path, &train->est_pos.position, &current_path_dest_pos, true
          // );
        }

        train_update_next_sensor(train, time);
        break;
      case STOPPED:
        // when the train is stopped there are 2 things that could be true:
        // 1. the train has reached it's final destination.
        // 2. the train has reached the end of the simple path that it's currently on.
        if (train->path_index == train->plan.paths_len - 1) {
          // release all previous nodes in our path
          for (int i = 0; i < train->plan.path.nodes_len; ++i) {
            struct TrackNode *node = train->plan.path.nodes[i];

            if (!(node->type == NODE_SENSOR || node->type == NODE_BRANCH || node->type == NODE_MERGE
                )) {
              continue;
            }

            int zone = node->zone;

            if (ZoneOccupied(zone) == train->train_index) {
              ReleaseReservations(terminal, zone);
            }
          }

          if (train->last_sensor_index != -1) {
            struct TrackNode *sensor = train->plan.path.nodes[train->last_sensor_index];
            // rereserve the node that we're currently on
            ReserveTrack(terminal, sensor->zone, train->train_index);
          }

          // check if we're on the last simple path in path and the next node is our final
          // destination
          // if so, we're done.
          if (train->pf_state == NONE) {
            train->active = false;
            break;
          }

          reroute_train(terminal, train_planner, train, time);
          break;
        }

        ++train->path_index;
        // TerminalLogPrint(terminal, "STOPPED TO PATH_BEGIN");
        train->state = PATH_BEGIN;
        break;
      case LOCKED:
        if (time - train->lock_begin_time >= DEADLOCK_DURATION) {
          train->lock_begin_time = time;

          // // TrainReverse(train_tid, train->train);
          // TODO: ?????
          train->pf_state = ROUTE_TO_SELECTED_DEST;
          train->selected_dest = train->plan.dest.node;
          train->est_pos.position.node = train->est_pos.position.node->reverse;
          // train->est_pos.position.node = train->est_pos.position.node->reverse;
          // if (train->pf_state = ROUTE_TO_SELECTED_DEST){
          //   reroute_train(terminal, train_planner, train);
          // }

          if (train->plan.path_found) {
            // TrainReverse(train_tid, train->train);
            TerminalLogPrint(terminal, "reverse 2");
            train->state = PATH_BEGIN;
          } else {
            train->pf_state = ROUTE_TO_SELECTED_DEST;
            reroute_train(terminal, train_planner, train, time);
          }
        }

        if (time - train->lock_begin_time >= WAIT_DURATION) {
          train->state = PATH_BEGIN;
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
// train 47 - A10
// train 54 - A5
// train 58 - C4
// train 77 - B9
// train 78 - C3
int initial_sensors[] = {0, 35, 14, 9, 4, 12, 24, 34};
int active_trains[] = {58};
int train_speeds[] = {10, 10, 10, 10, 7, 10, 10, 10};

// get train associated with sensor trigger. null if spurious.
struct Train *sensor_get_train(struct Train *trains, int sensor) {
  // check if previous train location update was this sensor.
  // if so, return null because it means the train is sitting on top of it still since the last
  // sensor trigger.
  // check initial sensors to see if they're for one of the trains that are inactive.
  // TODO: we currently just check if it's the initial train sensor or if the sensor is in a train's
  //   path.
  // TODO: use get_next_sensor to check whether this is an expected sensor for a train.

  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    struct Train *train = &trains[i];

    if (!train->active) {
      // if (initial_sensors[train->train_index] == sensor) {
      //   return train;
      // }

      continue;
    }

    if (train->next_sensor == &track[sensor]) {
      return train;
    }

    // for (int i = 0; i < train->plan.path.nodes_len; ++i) {
    //   if (train->plan.path.nodes[i] == &track[sensor]) {
    //     return train;
    //   }
    // }
  }

  return NULL;
}

void set_train_active(struct Train *train, int speed, int time) {
  train->active = true;
  train->speed = speed;
}

static void
train_update_pos_from_sensor(struct Train *train, struct TrackNode *sensor_node, int time) {
  struct TrackPosition sensor_pos = {.node = sensor_node, .offset = 0};
  struct TrainPosition sensor_train_pos = {.position = sensor_pos, .last_dir = DIR_AHEAD};
  // add offset to get the front of the train from sensor contact point
  struct TrainPosition train_pos =
      train->reversed
          ? train_position_add(sensor_train_pos, &train->plan.path, TRAIN_REVERSE_FRONT_OFFSET)
          : train_position_add(sensor_train_pos, &train->plan.path, TRAIN_FORWARD_FRONT_OFFSET);

  train->last_known_pos = train_pos;
  train->last_known_pos_time = time;

  train->est_pos = train_pos;
  train->est_pos_update_time = time;
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

    struct Train *train = sensor_get_train(train_states, sensor);
    if (train == NULL) {
      continue;
    }

    struct TrackNode *sensor_node = &track[sensor];
    // do not update location again if we've already seen the sensor.
    if (train->active && train->last_sensor_index != -1 &&
        train->plan.path.nodes[train->last_sensor_index] == sensor_node) {
      continue;
    }

    struct TrackNode *last_node = train->last_known_pos.position.node;
    if (last_node != NULL) {
      int zone = last_node->zone;
      TerminalLogPrint(terminal, "last_known_pos node %s", last_node->name);

      // release all previous nodes in our path before the last sensor node
      for (int i = 0; i <= train->last_sensor_index; ++i) {
        struct TrackNode *node = train->plan.path.nodes[i];

        if (!(node->type == NODE_SENSOR || node->type == NODE_BRANCH || node->type == NODE_MERGE)) {
          continue;
        }

        int zone = node->zone;

        if (ZoneOccupied(zone) == train->train_index) {
          ReleaseReservations(terminal, zone);
        }
      }

      if (ZoneOccupied(zone) == train->train_index) {
        ReleaseReservations(terminal, zone);
      }
    }

    train_update_pos_from_sensor(train, sensor_node, time);

    TerminalLogPrint(
        terminal, "\033[31mSensor hit %s attributed to Train %d.", sensor_node->name, train->train
    );

    if (!train->active) {
      // stop train prior to routing since it may take a while
      TrainSetSpeed(train_tid, train->train, 0);
      set_train_active(train, train_speeds[train->train_index], time);
      reroute_train(terminal, train_planner, train, time);
      TerminalLogPrint(terminal, "Train %d active", train->train);
      // train_log(terminal, train);
      continue;
    }

    if (!train->plan.path_found) {
      continue;
    }

    int current_sensor_index = get_next_sensor_index(train);
    if (current_sensor_index != -1) {
      train->last_sensor_index = current_sensor_index;
    }

    train->sensor_eta_error = train->next_sensor_eta - time;

    train_update_next_sensor(train, time);

    if (train->state == CONSTANT_VELOCITY) {
      struct SimplePath *cur_path = &train->plan.paths[train->path_index];
      struct TrackNode *current_path_dest = cur_path->dest.node;
      int current_path_dest_dir = cur_path->dest_dir;
      struct TrackPosition current_path_dest_pos = {
          .node = cur_path->dest.node, .offset = cur_path->dest.offset
          // .offset = current_path_dest->edge[current_path_dest_dir].dist + REVERSE_OVERSHOOT_DIST
      };

      if (current_path_dest == train->plan.dest.node) {
        current_path_dest_pos.offset = train->plan.dest.offset;
      }

      int dist_to_current_dest = get_distance_between(
          &train->plan.path, &train->est_pos.position, &current_path_dest_pos, true
      );
      int dist_to_stopping_dist =
          max(dist_to_current_dest - TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed],
              0);

      train->move_stop_time =
          time + get_constant_velocity_travel_time(train, dist_to_stopping_dist);
      TerminalLogPrint(terminal, "Update full move stop time: %d", train->move_stop_time);
    }
  }
}

void route_return(
    int terminal,
    int train_planner,
    int time,
    struct Train *train,
    struct TrackNode *initial_pos,
    struct TrackNode *dest
) {
  train->pf_state = ROUTE_TO_SELECTED_DEST;
  train->selected_dest = dest;

  // destination is a sensor. we reverse it to fake entering our next zone.
  ReserveTrack(terminal, initial_pos->reverse->zone, train->train_index);
  train->initial_pos = initial_pos;
  train_update_pos_from_sensor(train, initial_pos->reverse, time);

  set_train_active(train, train_speeds[train->train_index], time);
  reroute_train(terminal, train_planner, train, time);
}

void handle_route_return_req(
    int terminal,
    int clock_server,
    int train_tid,
    int train_planner,
    struct TrainManagerRouteReturnRequest *req,
    struct Train *trains
) {
  int time = Time(clock_server);
  enum Track selected_track = TrainGetSelectedTrack(train_tid);

  struct TrackNode *dest1 = &track[trainset_get_sensor_index(req->dest1)];
  struct Train *train1 = &trains[trainset_get_train_index(req->train1)];
  struct TrackNode *sensor1 = selected_track == TRACK_A ? &track[trainset_get_sensor_index("A5")]
                                                        : &track[trainset_get_sensor_index("A2")];

  route_return(terminal, train_planner, time, train1, sensor1, dest1);

  if (req->train2 != 0) {
    struct TrackNode *dest2 = &track[trainset_get_sensor_index(req->dest2)];
    struct Train *train2 = &trains[trainset_get_train_index(req->train2)];
    struct TrackNode *sensor2 = selected_track == TRACK_A ? &track[trainset_get_sensor_index("C3")]
                                                          : &track[trainset_get_sensor_index("A9")];

    route_return(terminal, train_planner, time, train2, sensor2, dest2);
  }
}

void handle_rand_route_req(
    int terminal,
    int clock_server,
    int train_tid,
    int train_planner,
    struct TrainManagerRandRouteRequest *req,
    struct Train *trains
) {
  int time = Time(clock_server);
  enum Track selected_track = TrainGetSelectedTrack(train_tid);

  struct Train *train1 = &trains[trainset_get_train_index(req->train1)];
  struct TrackNode *sensor1 = selected_track == TRACK_A ? &track[trainset_get_sensor_index("A5")]
                                                        : &track[trainset_get_sensor_index("A2")];

  // destination is a sensor. we reverse it to fake entering our next zone.
  ReserveTrack(terminal, sensor1->reverse->zone, train1->train_index);
  train_update_pos_from_sensor(train1, sensor1->reverse, time);

  train1->pf_state = RAND_ROUTE;
  set_train_active(train1, train_speeds[train1->train_index], time);
  reroute_train(terminal, train_planner, train1, time);

  if (req->train2 != 0) {
    struct Train *train2 = &trains[trainset_get_train_index(req->train2)];
    struct TrackNode *sensor2 = selected_track == TRACK_A ? &track[trainset_get_sensor_index("C3")]
                                                          : &track[trainset_get_sensor_index("A9")];
    // destination is a sensor. we reverse it to fake entering our next zone.
    ReserveTrack(terminal, sensor2->reverse->zone, train2->train_index);
    train_update_pos_from_sensor(train2, sensor2->reverse, time);

    train2->pf_state = RAND_ROUTE;
    set_train_active(train2, train_speeds[train2->train_index], time);
    reroute_train(terminal, train_planner, train2, time);
  }
}

void train_manager_task() {
  RegisterAs("train_manager");

  int clock_server = WhoIs("clock_server");
  int train_tid = WhoIs("train");
  int train_planner = WhoIs("train_planner");
  int terminal = WhoIs("terminal");

  struct Train trains[TRAINSET_NUM_TRAINS] = {0};

  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    int train_num = TRAINSET_TRAINS[i];
    train_init(&trains[i], train_num);
    train_update_terminal(terminal, &trains[i], 0);
  }

  int tid;
  struct TrainManagerRequest req;

  // TerminalUpdateZoneReservation(terminal, 5, 0, 0);

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

  // struct Train *train = &trains[trainset_get_train_index(54)];
  // train_init(train, 54);

  // // // TODO reverse plan iterations to positive
  // // A6 to BR 18 102 is 488mm?

  // // train_log(terminal, train);

  // int sensor_index = initial_sensors[train->train_index];
  // struct TrackNode *sensor_node = &track[16];
  // int time = Time(clock_server);

  // train->last_sensor_index = 0;
  // train->last_known_pos.position.node = sensor_node;
  // train->last_known_pos.position.offset = 5;
  // train->last_known_pos.last_dir = DIR_STRAIGHT;
  // train->last_known_pos_time = time;

  // train->est_pos.position.node = sensor_node;
  // train->est_pos.position.offset = 5;
  // train->est_pos.last_dir = DIR_STRAIGHT;
  // train->est_pos_update_time = time;

  // set_train_active(train, train_speeds[train->train_index], time);
  // train->pf_state = RAND_ROUTE;
  // reroute_train(terminal, train_planner, train);

  // // route_plan_process(&train->plan);

  // train->last_sensor_index = train->plan.path.nodes_len - 1;
  // // train_log(terminal, train);

  // if (train->plan.path_found) {
  //   TerminalLogPrint(terminal, "Path found!");
  // } else {
  //   TerminalLogPrint(terminal, "Path not found!");
  // }

  // for (unsigned int i = 0; i < train->plan.paths_len; ++i) {
  //   struct SimplePath path = train->plan.paths[i];
  //   TerminalLogPrint(
  //       terminal,
  //       "start: %d, end: %d, reverse: %d, dest: %s +%d\r\n",
  //       path.start_index,
  //       path.end_index,
  //       path.reverse,
  //       path.dest.node->name,
  //       path.dest.offset
  //   );

  //   for (int j = path.start_index; j <= path.end_index; ++j) {
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

  // TerminalLogPrint(terminal, "after print path");

  // int dist_to_current_dest =
  //     get_distance_between(&train->plan.path, &train->plan.src, &train->plan.dest, true);
  // TerminalLogPrint(
  //     terminal,
  //     "distance between %s, offset: %d and %s offset: %d is %d",
  //     train->plan.src.node->name,
  //     train->plan.src.offset,
  //     train->plan.dest.node->name,
  //     train->plan.dest.offset,
  //     dist_to_current_dest
  // );

  // for (int i = 0; i < train->plan.path.nodes_len; ++i) {
  //       TerminalLogPrint(
  //           WhoIs("terminal"), "node %s direction %d", train->plan.path.nodes[i]->name,
  //           train->plan.path.directions[i]
  //       );
  //     }

  // struct TrackPosition broken = {.node = &track[16]};
  // int broken_dist = get_distance_between(&train->plan.path, &broken, &train->plan.dest, true);

  // TerminalLogPrint(terminal, "BROKEN DIST %d", broken_dist);

  // while (true) {}

  // TerminalLogPrint(
  //     terminal, "orig: %s +%d", train->est_pos.position.node->name,
  //     train->est_pos.position.offset
  // );

  // train_update_terminal(terminal, train, time);

  // TerminalLogPrint(terminal, "deceleration %d", get_train_decel(train));
  // struct TrainPosition new_pos = train_position_reverse_node(train->est_pos, train_tid);
  // TerminalLogPrint(terminal, "new_pos: %s +%d", new_pos.position.node->name,
  // new_pos.position.offset);

  // for (int i = 0; i < 1000; i += 50) {
  //   TerminalLogPrint(terminal, "shortmove duration for %d to move %d", shortmove_get_duration(58,
  //   10, i), i);
  // }

  // enum Track selected_track = TrainGetSelectedTrack(train_tid);

  // struct Train *train1 = &trains[trainset_get_train_index(54)];
  // struct TrackNode *sensor1 = selected_track == TRACK_A ? &track[trainset_get_sensor_index("A5")]
  //                                                       :
  //                                                       &track[trainset_get_sensor_index("E8")];

  // // destination is a sensor. we reverse it to fake entering our next zone.
  // ReserveTrack(terminal, sensor1->reverse->zone, train1->train_index);
  // train_update_pos_from_sensor(train1, sensor1->reverse, Time(clock_server));

  // train1->pf_state = RAND_ROUTE;
  // set_train_active(train1, train_speeds[train1->train_index], Time(clock_server));
  // reroute_train(terminal, train_planner, train1);

  // handle_tick(terminal, train_tid, train_planner, clock_server, trains);

  Create(NOTIFIER_PRIORITY, train_manager_tick_notifier);

  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case TRAIN_MANAGER_ROUTE_RETURN:
        handle_route_return_req(
            terminal, clock_server, train_tid, train_planner, &req.route_return_req, trains
        );
        Reply(tid, NULL, 0);
        break;
      case TRAIN_MANAGER_RAND_ROUTE:
        handle_rand_route_req(
            terminal, clock_server, train_tid, train_planner, &req.rand_route_req, trains
        );
        Reply(tid, NULL, 0);
        break;
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

void TrainManagerRouteReturn(int tid, int train1, int train2, char *sensor, char *sensor2) {
  struct TrainManagerRequest req = {
      .type = TRAIN_MANAGER_ROUTE_RETURN,
      .route_return_req = {.train1 = train1, .train2 = train2, .dest1 = sensor, .dest2 = sensor2}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainManagerRouteOneReturn(int tid, int train, char *sensor) {
  struct TrainManagerRequest req = {
      .type = TRAIN_MANAGER_ROUTE_RETURN,
      .route_return_req = {.train1 = train, .train2 = 0, .dest1 = sensor, .dest2 = NULL}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainManagerRandomlyRoute(int tid, int train1, int train2) {
  struct TrainManagerRequest req = {
      .type = TRAIN_MANAGER_RAND_ROUTE, .rand_route_req = {.train1 = train1, .train2 = train2}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainManagerUpdateSensors(int tid, bool *sensors) {
  struct TrainManagerRequest req = {
      .type = TRAIN_MANAGER_UPDATE_SENSORS, .update_sens_req = {.sensors = sensors}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
