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
  PATH_BEGIN,
  SHORT_MOVE,
  ACCELERATING,
  CONSTANT_VELOCITY,
  DECELERATING,
  SHORT_MOVE_DECELERATING,
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
  unsigned int path_index;

  struct TrackPosition est_pos;
  int est_pos_update_time;
  struct TrackPosition last_known_pos;
  int last_known_pos_time;

  int speed;

  int next_sensor_eta;
  int sensor_eta_error;

  FixedPointInt acceleration;
  FixedPointInt velocity;
};

// ticks to wait after a short move.
static int SHORT_MOVE_DELAY = 400;

// TODO: if train hits any sensor after a node in the path, this won't work because it cannot find
// the destinaton. theoretically, this should never happen because the train should always stop at
// the destination.
static int
get_distance_between(struct Path *path, struct TrackPosition *src, struct TrackPosition *dest) {
  int dist = -src->offset + dest->offset;

  int pos1_index = -1;
  // find pos1 index in path
  for (int i = path->nodes_len - 1; i >= 0; --i) {
    if (src->node == path->nodes[i] || src->node == path->nodes[i]->reverse) {
      pos1_index = i;
      break;
    }
  }

  if (pos1_index == -1) {
    return -1;
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
  }

  if (train->active) {
    TerminalLogPrint(terminal, "decel_begin_time: %d", train->decel_begin_time);
    TerminalLogPrint(terminal, "decel_duration: %d", train->decel_duration);
    TerminalLogPrint(terminal, "last_sensor_index: %d", train->last_sensor_index);
    TerminalLogPrint(terminal, "move_start_time: %d", train->move_start_time);
    TerminalLogPrint(terminal, "move_duration: %d", train->move_duration);
    TerminalLogPrint(terminal, "move_stop_time %d", train->move_stop_time);
    TerminalLogPrint(
        terminal, "est_pos: %s, offset: %d", train->est_pos.node->name, train->est_pos.offset
    );
    TerminalLogPrint(terminal, "est_pos_update_time: %d", train->est_pos_update_time);
  }
}

static void train_update_terminal(int terminal, struct Train *train) {
  char *pos_node;
  int pos_offset = 0;
  char *state;
  char *next_sensor = "N/A";
  int sensor_estimate = 0;
  char *dest;

  switch (train->state) {
    case PATH_BEGIN:
      state = "PATH_BEGIN";
      break;
    case SHORT_MOVE:
      state = "SHORT_MOVE";
      break;
    case ACCELERATING:
      state = "ACCELERATING";
      break;
    case CONSTANT_VELOCITY:
      state = "CONSTANT_VELOCITY";
      break;
    case DECELERATING:
      state = "DECELERATING";
      break;
    case SHORT_MOVE_DECELERATING:
      state = "SHORT_MOVE_DECELERATING";
      break;
    case STOPPED:
      state = "STOPPED";
      break;
    case WAITING_FOR_INITIAL_POS:
      state = "WAITING_FOR_INITIAL_POS";
      break;
  }

  if (train->state != WAITING_FOR_INITIAL_POS) {
    pos_node = train->est_pos.node->name;
    pos_offset = train->est_pos.offset;

    if (train->plan.path_found) {
      dest = train->plan.dest.node->name;
    } else {
      dest = "N/A";
    }
  } else {
    pos_node = "N/A";
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

  train->est_pos_update_time = 0;
  train->last_known_pos_time = 0;

  train->next_sensor_eta = 0;
  train->sensor_eta_error = 0;

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

// static struct TrackPosition
// plan_get_stopping_pos(struct Train *train, struct Path *path, struct TrackPosition *dest) {
//   // distance from train position
//   int stopping_offset = get_distance_between(path, &train->est_pos, dest);

//   struct TrackNode *last_node = NULL;
//   int offset_from_last_node = stopping_offset;
//   // traverse path to see which node is before the stopping_offset
//   for (int i = path->nodes_len - 1; i >= 0; --i) {
//     struct TrackNode *cur_node = path->nodes[i];
//     int edge_dir = path->directions[i];
//     int edge_dist = cur_node->edge[edge_dir].dist;

//     if (offset_from_last_node < edge_dist) {
//       last_node = cur_node;
//       break;
//     }

//     offset_from_last_node -= edge_dist;
//   }

//   // our offset is not covered by the edge after the last node
//   if (last_node == NULL) {
//     // set to last node
//     last_node = path->nodes[0];
//   }

//   struct TrackPosition pos = {.node = last_node, .offset = offset_from_last_node};
//   return pos;
// }

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

inline static int calculate_travel_time(FixedPointInt initial_velocity, FixedPointInt accel, int dist) {
  // d = initial_velocity * t + 1/2 * accel * time^2
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

// returns time to move distance for a full acceleration to constant velocity move
int get_move_time(struct Train *train, int distance) {
  distance -= TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed];

  int constant_velocity_time = get_constant_velocity_travel_time(
      train, (distance - TRAINSET_ACCEL_DISTANCES[train->train_index][train->speed])
  );

  return TRAINSET_ACCEL_TIMES[train->train_index][train->speed] + constant_velocity_time;
}

inline int get_dist_travelled(FixedPointInt initial_velocity, FixedPointInt accel, int time) {
  // d = v0 * t + 1/2 * at^2
  return fixed_point_int_get(initial_velocity * time + (accel * time * time) / 2);
}

inline int get_dist_constant_velocity(FixedPointInt velocity, int time) {
  return fixed_point_int_get(velocity * time);
}

int shortmove_get_velocity(struct Train *train, int dist) {
  // assuming constant velocity, add to account for deceleration time
  int time_delta = shortmove_get_duration(train->train, train->speed, dist) + SHORT_MOVE_DELAY;
  return fixed_point_int_from(dist) / time_delta;
}

// distance moved since last current pos update
// int get_train_dist_travelled_full_move(struct Train *train, int time) {
//   int total_move_duration = time - train->move_start_time;
//   int last_update_total_move_duration = train->est_pos_update_time - train->move_start_time;

//   // 3 scenarios:
//   // - position updated in the middle of constant velocity.
//   // - position updated in the middle of acceleration
//   //   - still accelerating
//   //   - reached constant velocity.

//   // position updated in the middle of constant velocity.
//   if (train->last_known_pos_time - train->move_start_time >
//       TRAINSET_ACCEL_TIMES[train->train_index][train->speed]) {
//     TerminalLogPrint(train->terminal, "position updated in middle of const velocity");
//     // last pos update happened after constant velocity was reached.
//     return get_dist_constant_velocity(
//         TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed],
//         time - train->last_known_pos_time
//     );
//   }

//   // assume constant acceleration
//   int accel = get_train_accel(train);

//   // position updated in the middle of acceleration
//   // still accelerating
//   if (time - train->move_start_time <= TRAINSET_ACCEL_TIMES[train->train_index][train->speed]) {
//     TerminalLogPrint(
//         train->terminal,
//         "still accelerating %d duration: %d",
//         get_dist_travelled(0, accel, time - train->move_start_time),
//         time - train->move_start_time
//     );
//     // get distance from last known position
//     return get_dist_travelled(0, accel, time - train->move_start_time) -
//            get_dist_travelled(0, accel, time - train->last_known_pos_time);
//   }

//   int begin_constant_velocity_time =
//       train->move_start_time + TRAINSET_ACCEL_TIMES[train->train_index][train->speed];

//   // reached constant speed
//   // full acceleration dist since move began - distance travelled since move began to last known
//   pos int accel_dist =
//       get_dist_travelled(0, accel, begin_constant_velocity_time - train->move_start_time) -
//       get_dist_travelled(0, accel, train->last_known_pos_time - train->move_start_time);

//   // constant velocity duration
//   int constant_velocity_duration = time - begin_constant_velocity_time;
//   int constant_velocity_dist = get_dist_constant_velocity(
//       TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed], constant_velocity_duration
//   );

//   TerminalLogPrint(train->terminal, "constant velocity dist %d\r\n", constant_velocity_dist);

//   return accel_dist + constant_velocity_dist;
// }

// int get_train_dist_travelled_decel(struct Train *train, int time) {
//   // assuming constant deceleration
//   FixedPointInt decel = -TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed] /
//                         TRAINSET_DECEL_TIMES[train->train_index][train->speed];
//   // two scnenarios:
//   // - last known pos is from while the train was accelerating or going at
//   //   constant velocity
//   // - last known pos is from while the train was decelerating.

//   // last known pos is from while the train was accelerating or going at constant velocity
//   if (train->last_known_pos_time < train->decel_begin_time) {
//     // distance travelled while at constant velocity or accelerating
//     int accel_dist = get_train_dist_travelled_full_move(train, train->decel_begin_time);
//     int decel_dist = get_dist_travelled(
//         TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed],
//         decel,
//         time - train->decel_begin_time
//     );

//     return accel_dist + decel_dist;
//   }

//   // last known pos is from while the train was decelerating.
//   int decel_dist_to_known_pos = get_dist_travelled(
//       TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed],
//       decel,
//       train->last_known_pos_time - train->decel_begin_time
//   );
//   int total_decel_dist = get_dist_travelled(
//       TRAINSET_MEASURED_SPEEDS[train->train_index][train->speed],
//       decel,
//       time - train->decel_begin_time
//   );

//   // assuming constant deceleration from max velocity to 0
//   return total_decel_dist - decel_dist_to_known_pos;
// }

void update_train_pos(int terminal, int time, struct Train *train) {
  if (train->state == STOPPED || train->state == WAITING_FOR_INITIAL_POS ||
      train->state == PATH_BEGIN) {
    return;
  }

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
      if (train->move_stop_time <= time) {
        train->est_pos = track_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(
                train->velocity, 0, train->move_stop_time - train->est_pos_update_time
            )
        );
        train->est_pos_update_time = train->move_stop_time;
      } else {
        // assume constant velocity for short moves
        train->est_pos = track_position_add(
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
        train->est_pos = current_path_dest_pos;
        train->est_pos_update_time = time;

        int decel_end_time = train->decel_begin_time + train->decel_duration;

        train->est_pos = track_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(train->velocity, 0, decel_end_time - train->est_pos_update_time)
        );
        train->est_pos_update_time = decel_end_time;
      } else {
        // TerminalLogPrint(terminal, "short move const velocity %d, time diff: %d, dist: %d",
        // train->velocity, time - train->est_pos_update_time, get_dist_travelled(train->velocity,
        // 0, time - train->est_pos_update_time)); constant velocity
        train->est_pos = track_position_add(
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

        train->est_pos = track_position_add(
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
        train->est_pos = track_position_add(
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
        train->est_pos = track_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(
                train->velocity, 0, train->move_stop_time - train->est_pos_update_time
            )
        );
        train->est_pos_update_time = train->move_stop_time;
      } else {
        train->est_pos = track_position_add(
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

        train->est_pos = track_position_add(
            train->est_pos,
            &train->plan.path,
            get_dist_travelled(train->velocity, 0, decel_end_time - train->est_pos_update_time)
        );
        train->est_pos_update_time = decel_end_time;
        // we don't set velocity here because when state changes it gets set to 0.
      } else {
        train->est_pos = track_position_add(
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

bool has_reached_dest(struct Train *train, struct TrackPosition *dest) {
  return get_distance_between(&train->plan.path, &train->est_pos, dest) <= 0;
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
  struct TrackPosition rand_dest = track_position_random(terminal);
  train->plan = CreatePlan(train_planner, &train->est_pos, &rand_dest);

  while (!train->plan.path_found) {
    TerminalLogPrint(
        terminal,
        "No path to %s from %s, trying to find another destination.",
        rand_dest.node->name,
        train->est_pos.node->name
    );
    rand_dest = track_position_random(terminal);
    train->plan = CreatePlan(train_planner, &train->est_pos, &rand_dest);
  }

  TerminalLogPrint(terminal, "before printing dest addr %d", train->plan.dest.node);
  TerminalLogPrint(terminal, "request dest addr %d", rand_dest.node);
  TerminalLogPrint(terminal, "requested dest is %s", rand_dest.node->name);
  TerminalLogPrint(terminal, "tryin to print plan dest %d", train->plan.dest.node->num);

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
        "start: %d, end: %d, reverse: %d\r\n",
        path.start_index,
        path.end_index,
        path.reverse
    );

    for (int j = path.start_index; j >= path.end_index; --j) {
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
  // if the train is currently on a sensor, then the path starts at a sensor, and we set
  // last sensor triggered to the first node.
  train->last_sensor_index = -1;
  train->last_switch_index = -1;
  train->state = PATH_BEGIN;

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

    // train_update_terminal(terminal, train);

    int time = Time(clock_server);

    update_train_pos(terminal, time, train);
    struct Path *path = &train->plan.path;

    // TODO:
    // - make sure we are within reservation range ^

    // when we are close enough to the next switch, activate it.
    int next_switch_index = get_next_switch_index(train);
    // we check if next switch is equal to last switch in case we already activated
    // the switch previously.
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

      // TODO: check for reservation
      if (get_distance_between(path, &train->est_pos, &next_switch_pos) <= FLIP_SWITCH_DIST) {
        switch (path->directions[next_switch_index]) {
          case DIR_CURVED:
            TerminalLogPrint(
                terminal,
                "\033[33mset switch %d to CURVED, estimated dist to switch: %d",
                next_switch->num,
                get_distance_between(path, &train->est_pos, &next_switch_pos)
            );
            break;
          case DIR_AHEAD:
            TerminalLogPrint(
                terminal,
                "\033[33mset switch %d to STRAIGHT estimated dist to switch: %d",
                next_switch->num,
                get_distance_between(path, &train->est_pos, &next_switch_pos)
            );
        }
        TrainSetSwitchDir(
            train_tid,
            next_switch->num,
            path->directions[next_switch_index] == DIR_AHEAD ? TRAINSET_DIRECTION_STRAIGHT
                                                             : TRAINSET_DIRECTION_CURVED
        );
        train->last_switch_index = next_switch_index;
      }
    }

    // location updates and reservations.
    int next_sensor_index = get_next_sensor_index(train);
    if (next_sensor_index != -1) {
      struct TrackNode *next_sensor = path->nodes[next_sensor_index];
      struct TrackPosition next_sensor_pos = {.node = next_sensor, .offset = 0};

      // TODO: to get time to next sensor:
      //       - when state is constant velocity, trivial
      //       - when we're in the middle of acceleration, use kinematics equation
      //       - when in the middle of short move, get short move velocity and assume constant
      //       velocity.

      // TODO: update time to next sensor and distance
      // int dist = get_distance_between(&train->plan.path, &train->est_pos, &next_sensor_pos);
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

    int dist_to_current_dest = get_distance_between(path, &train->est_pos, &current_path_dest_pos);

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

          train->velocity = 0;
          train->acceleration = 0;
          train->state = STOPPED;
        }
        break;
      case PATH_BEGIN:
        // check if move is a reverse
        if (current_path.reverse) {
          TrainReverseInstant(train_tid, train->train);
          train->est_pos.node = train->est_pos.node->reverse;
          // TODO: account for length of train.
          train->est_pos.offset = 0;
          TerminalLogPrint(terminal, "state change to STOPPED from PATH_BEGIN");
          TerminalLogPrint(terminal, "reversing");
          train->state = STOPPED;
          break;
        }

        TerminalLogPrint(
            terminal,
            "BEGINNING NEW PATH DEST: %s with offset %d",
            current_path_dest_pos.node->name,
            current_path_dest_pos.offset
        );

        // short move (possibly add a buffer additionally?)
        if (dist_to_current_dest < TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed] +
                                       TRAINSET_ACCEL_DISTANCES[train->train_index][train->speed]) {
          train->move_start_time = time;
          train->move_duration =
              shortmove_get_duration(train->train, train->speed, dist_to_current_dest);
          train->move_stop_time = train->move_start_time + train->move_duration;

          TerminalLogPrint(terminal, "state change to SHORT_MOVE from PATH_BEGIN");
          train->state = SHORT_MOVE;
          train->velocity = shortmove_get_velocity(train, dist_to_current_dest);
          train->acceleration = 0;

          TrainSetSpeed(train_tid, train->train, train->speed);
          TerminalLogPrint(
              terminal,
              "Train %d, performing a short move of %dmm to %s for %d ticks",
              train->train,
              dist_to_current_dest,
              current_path_dest->name,
              train->move_duration
          );
        } else {
          train->move_start_time = time;
          train->move_duration = get_move_time(train, dist_to_current_dest);
          train->move_stop_time = train->move_start_time + train->move_duration;
          TerminalLogPrint(terminal, "state change to ACCELERATING from PATH_BEGIN");
          train->state = ACCELERATING;
          train->acceleration = get_train_accel(train);

          TrainSetSpeed(train_tid, train->train, train->speed);
          TerminalLogPrint(
              terminal,
              "Train %d, performing a max velocity move of %dmm to %s for %d ticks",
              train->train,
              dist_to_current_dest,
              current_path_dest->name,
              train->move_duration,
              time
          );
        }
        break;
      case STOPPED:
        // when the train is stopped there are 2 things that could be true:
        // 1. the train has reached it's final destination.
        // 2. the train has reached the end of the simple path that it's currently on.

        // check if we're on the last simple path in path and the next node is our final destination
        // if so, we're done.
        if (train->path_index == train->plan.paths_len - 1) {
          route_train_randomly(terminal, train_planner, train);
          train->last_sensor_index = train->plan.path.nodes_len - 1;
          break;
        }

        ++train->path_index;
        TerminalLogPrint(terminal, "STOPPED TO PATH_BEGIN");
        train->state = PATH_BEGIN;
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
// train 54 - A5
// train 58 - C4
// train 77 - B9
// train 78 - C3
int initial_sensors[] = {0, 35, 14, 9, 4, 12, 24, 34};
int active_trains[] = {58};
int train_speeds[] = {10, 10, 10, 10, 10, 10, 10, 10};

// get train associated with sensor trigger. null if spurious.
struct Train *sensor_get_train(struct Train *trains, int sensor) {
  // check if previous train location update was this sensor.
  // if so, return null because it means the train is sitting on top of it still since the last
  // sensor trigger.
  // check initial sensors to see if they're for one of the trains that are inactive.
  // TODO: we currently just check if it's the initial train sensor or if the sensor is in a train's
  //   path.

  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    struct Train *train = &trains[i];

    if (!train->active) {
      if (initial_sensors[train->train_index] == sensor) {
        return train;
      }

      continue;
    }

    for (int i = train->plan.path.nodes_len - 1; i >= 0; --i) {
      if (train->plan.path.nodes[i] == &track[sensor]) {
        return train;
      }
    }
  }

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

    struct Train *train = sensor_get_train(train_states, sensor);
    if (train == NULL) {
      continue;
    }

    struct TrackNode *sensor_node = &track[sensor];
    // do not update location again if we've already seen the sensor.
    if (train->plan.path.nodes[train->last_sensor_index] == sensor_node) {
      continue;
    }

    // update train position
    train->last_known_pos.node = sensor_node;
    train->last_known_pos.offset = 0;
    train->last_known_pos_time = time;

    train->est_pos.node = sensor_node;
    train->est_pos.offset = 0;
    train->est_pos_update_time = time;

    TerminalLogPrint(
        terminal, "\033[31mSensor hit %s attributed to Train %d.", sensor_node->name, train->train
    );

    if (!train->active) {
      // stop train prior to routing since it may take a while
      TrainSetSpeed(train_tid, train->train, 0);
      set_train_active(train, train_speeds[train->train_index]);
      route_train_randomly(terminal, train_planner, train);
      train->last_sensor_index = train->plan.path.nodes_len - 1;
      TerminalLogPrint(terminal, "Train active....");
      train_log(terminal, train);
      continue;
    }

    if (!train->plan.path_found) {
      continue;
    }

    struct TrackPosition sensor_pos = {.node = sensor_node, .offset = 0};

    TerminalLogPrint(
        terminal,
        "\033[31mSensor estimation error: distance %d",
        get_distance_between(&train->plan.path, &sensor_pos, &train->est_pos)
    );

    int next_sensor_index = get_next_sensor_index(train);
    if (next_sensor_index != -1) {
      train->last_sensor_index = next_sensor_index;
    }

    // TODO: only set speed if we haven't already seen this sensor
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

      int dist_to_current_dest =
          get_distance_between(&train->plan.path, &train->est_pos, &current_path_dest_pos);
      int dist_to_stopping_dist =
          max(dist_to_current_dest - TRAINSET_STOPPING_DISTANCES[train->train_index][train->speed],
              0);

      train->move_stop_time =
          time + get_constant_velocity_travel_time(train, dist_to_stopping_dist);
      TerminalLogPrint(terminal, "Update full move stop time: %d", train->move_stop_time);
    }
  }
}

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
    train_update_terminal(terminal, &trains[i]);
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
  // struct TrackNode *sensor_node = &track[4];
  // int time = Time(clock_server);

  // train->last_sensor_index = 0;
  // train->last_known_pos.node = sensor_node;
  // train->last_known_pos.offset = 0;
  // train->last_known_pos_time = time;

  // train->est_pos.node = sensor_node;
  // train->est_pos.offset = 0;
  // train->est_pos_update_time = time;

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

  // TerminalLogPrint(terminal, "orig: %s +%d", train->est_pos.node->name, train->est_pos.offset);
  // // TerminalLogPrint(terminal, "deceleration %d", get_train_decel(train));
  // struct TrackPosition new_pos = track_position_subtract(train->est_pos, &train->plan.path, 300);
  // TerminalLogPrint(terminal, "new_pos: %s +%d", new_pos.node->name, new_pos.offset);

  // handle_tick(terminal, train_tid, train_planner, clock_server, trains);

  // TODO: - TEST track_position_subtract
  //       - sensor time estimate
  //       - train position map?
  //       - investigate reverses not working (dead spot?)
  //       - path to E5 from EX1 (throws exception)

  // TODO when we reverse, flip est_pos.
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

void RerouteTrain(int train, struct TrackPosition pos) {}

void TrainManagerUpdateSensors(int tid, bool *sensors) {
  struct TrainManagerRequest req = {
      .type = TRAIN_MANAGER_UPDATE_SENSORS, .update_sens_req = {.sensors = sensors}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

// LEFT TO DO:
// - reservations as we move
// - check for reservations in our path.
// - reroute train command.
