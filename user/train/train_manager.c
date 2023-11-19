#include "train_manager.h"

#include "selected_track.h"
#include "syscall.h"
#include "track_position.h"
#include "train_planner.h"
#include "train_sensor_notifier.h"
#include "trainset.h"
#include "trainset_task.h"
#include "user/server/name_server.h"
#include "user/terminal/terminal_task.h"

enum TrainState {
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

  int train;
  int train_index;

  enum TrainState state;
  int move_start_time;
  int move_duration;

  struct RoutePlan plan;
  // index of current path in route plan
  int path_index;
  // index of next node in current path
  int path_node_index;

  struct TrackPosition current_pos;
  struct TrackPosition last_known_pos;
  int last_known_pos_time;

  // int acceleration;
  int speed;
};

void train_init(struct Train *train, int train_num) {
  train->active = false;

  train->train = train_num;
  train->train_index = trainset_get_train_index(train_num);

  train->state = WAITING_FOR_INITIAL_POS;
  train->move_start_time = 0;
  train->move_duration = 0;

  train->plan.path_found = false;
  train->path_index = 0;
  train->path_node_index = 0;

  train->last_known_pos_time = 0;

  train->speed = 0;
}

// static int get_travel_time_between(
//     struct Path *path,
//     struct Train *state,
//     struct TrackPosition *pos1,
//     struct TrackPosition *pos2
// ) {
//   // mm
//   int dist = get_distance_between(path, pos1, pos2);

//   // in ticks (per 10ms)
//   return fixed_point_int_from(dist) / TRAINSET_MEASURED_SPEEDS[state->train_index][state->speed];
// }

// static struct TrackPosition plan_get_stopping_pos(struct Plan *plan, struct Train *state) {
//   struct Path *path = &plan->path;
//   // distance from train position
//   int stopping_offset = path->distance - state->last_known_pos.offset + plan->dest.offset -
//                         TRAINSET_STOPPING_DISTANCES[state->train_index][state->speed];

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
//     last_node = plan->path.nodes[0];
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

// returns time to move distance for a short move.
int get_short_move_time(struct Train *train, int distance) {
  return 0;
}

// returns time to move distance for a full acceleration to constant velocity move
int get_move_time(struct Train *train, int distance) {}

// returns distance needed to acelerate to constant velocity
int get_accel_dist() {
  return 0;
}

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

static void handle_tick(int terminal, int train_tid, int clock_server, struct Train *train_states) {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    struct Train *train = &train_states[i];

    if (!train->active) {
      continue;
    }

    // trains should always have a route plan. if they do not have a route plan they are not active.

    // short moves will be calculated based on time. we set time to be y = 1, 2... and measure x
    // distance travelled. this means that if we want to go x distance we will set speed for y time.

    struct Path *path = &train->plan.paths[train->path_index];

    // go through RoutePlan.
    // - when we are close enough to switch, activate it.
    // - make sure we are within reservation range ^
    // - when our stopping distance + distance to accelerate to constant velocity
    //   is greater than the distance we need to travel, use a short move.

    struct SimplePath current_path = train->plan.paths[train->path_index];
    // destination node of simple path
    struct TrackNode *current_path_dest = train->plan.path.nodes[current_path.end_index];
    struct TrackPosition current_path_dest_pos = {.node = current_path_dest, .offset = 0};

    if (current_path_dest == train->plan.dest.node) {
      current_path_dest_pos.offset = train->plan.dest.offset;
    }

    int dist_to_current_dest = get_distance_between(path, &train->current_pos, current_path_dest);

    switch (train->state) {
      case WAITING_FOR_INITIAL_POS:
        break;
      case SHORT_MOVE:
        // check if it's time to stop to reach destination
        if (train->move_start_time + train->move_duration >= Time(clock_server)) {
          TrainSetSpeed(train_tid, train->train, 0);
        }

        break;
      case ACCELERATING:
        break;
      case CONSTANT_VELOCITY:
        // check if it's time to stop to reach destination
        if (train->move_start_time + train->move_duration >= Time(clock_server)) {
          TrainSetSpeed(train_tid, train->train, 0);
        }

        train->state = STOPPED;
        break;
      case DECELERATING:
        break;
      case STOPPED:
        // check if we're on the last simple path in path and the next node is our final destination
        // if so, we're done.
        if (train->path_index == train->plan.paths_len - 1 &&
            train->path_node_index == train->plan.path.nodes_len - 1 &&
            // if we're at the destination or passed it
            dist_to_current_dest <= 0) {
          // TODO: find new path for train.
          break;
        }

        // short move (possibly add a buffer additionally?)
        if (dist_to_current_dest >= stopping_distance + dist_to_accelerate) {
          train->move_start_time = Time(clock_server);
          train->move_duration = get_short_move_time(train, dist_to_current_dest);
        } else {
          // accelerate to constant velocity
          TrainSetSpeed(train_tid, train->speed, 0);
          train->move_start_time = Time(clock_server);
          train->move_duration = get_move_time(train, dist_to_current_dest);
          train->state = ACCELERATING;

          // TODO: move to next path in route plan.
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

void route_train_randomly(int terminal, int train_planner, struct Train *train) {
  struct TrackPosition rand_dest = track_position_random();
  train->plan = CreatePlan(train_planner, &train->current_pos, &rand_dest);

  while (!train->plan.path_found) {
    TerminalLogPrint("No path to %s, trying to find another destination.", rand_dest.node->name);
    rand_dest = track_position_random();
    train->plan = CreatePlan(train_planner, &train->current_pos, &rand_dest);
  }

  TerminalLogPrint("Routing train %d to %s.", train->train, rand_dest.node->name);
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
    train->last_known_pos_time = time;

    train->current_pos.node = sensor_node;
    train->current_pos.offset = 0;

    if (!train->active) {
      set_train_active(train, train_speeds[train->train_index]);
      route_train_randomly(terminal, train_planner, train);
    }

    TerminalLogPrint(
        terminal, "Sensor hit %s attributed to Train %d.", sensor_node->name, train->train
    );
  }

  // // no train being routed
  // if (current_train == 0) {
  //   return;
  // }

  // // currently can only route one train at a time.
  // struct TrainState *trainstate = &train_states[trainset_get_train_index(current_train)];
  // struct Plan *plan = &trainstate->plan;

  // int triggered_sensor = -1;
  // for (int i = 0; i < TRAINSET_SENSORS_LEN; ++i) {
  //   // TODO: support for multiple trains.
  //   // for now we stop as soon as we see a sensor has been triggered
  //   // need to associate sensor with train for multiple trains.
  //   if (req->sensors[i]) {
  //     triggered_sensor = i;
  //     break;
  //   }
  // }

  // if (triggered_sensor == -1) {
  //   return;
  // }

  // trainstate->last_known_pos.node = &track[triggered_sensor];
  // trainstate->last_known_pos.offset = 0;

  // trainstate->direction = trainstate_get_direction(trainstate);

  // switch (plan->state) {
  //   case PLAN_NONE:
  //     break;
  //   case PLAN_ACCELERATING:
  //     // check if time since start of acceleration has been long enough
  //     if (Time(clock_server) - plan->state_update_time >= ACCELERATION_DURATION) {
  //       // TODO: should figure out if the dest offset is past another sensor, if it is we should
  //       // move dest.node to be the last sensor. that is, the sensor right before dest.node +
  //       offset
  //       // should also figure out negative offset and convert it to positive offset.
  //       plan->path = get_shortest_path(trainstate, plan->dest.node);

  //       struct Path *path = &plan->path;

  //       TerminalLogPrint(terminal, "Calculated path, distance: %d", path->distance);
  //       TerminalLogPrint(terminal, "Total of %d nodes in path", path->nodes_len);

  //       path_set_switches(&plan->path, train_tid);
  //       plan->state = PLAN_WAITING_TO_STOP;
  //       plan->state_update_time = Time(clock_server);

  //       // update stopping point
  //       plan->stopping_pos = plan_get_stopping_pos(plan, trainstate);
  //       TerminalLogPrint(
  //           terminal,
  //           "Stopping point node %s with offset %d",
  //           plan->stopping_pos.node->name,
  //           plan->stopping_pos.offset
  //       );

  //       plan->time_to_stop =
  //           Time(clock_server) +
  //           get_travel_time_between(
  //               &plan->path, trainstate, &trainstate->last_known_pos, &plan->stopping_pos
  //           );
  //       TerminalLogPrint(
  //           terminal, "Initial estimated time to stop (ticks): %d", plan->time_to_stop
  //       );

  //       // the first node in our path is always a sensor.
  //       plan->last_sensor_index = plan->path.nodes_len - 1;
  //     }
  //     break;
  //   case PLAN_CALCULATING_PATH:
  //     // TODO: move to different task?
  //     break;
  //   case PLAN_WAITING_TO_STOP: {
  //     // update terminal with time to reach next sensor
  //     // search path for previous sensor triggered
  //     plan->last_sensor_index = get_next_sensor(plan, &plan->path);

  //     if (plan->next_sensor_eta != 0) {
  //       TerminalLogPrint(
  //           terminal,
  //           "Estimated sensor ETA error (ticks): %d",
  //           Time(clock_server) - plan->next_sensor_eta
  //       );
  //     }

  //     int next_sensor_index = get_next_sensor(plan, &plan->path);
  //     int travel_time = get_travel_time_between(
  //         &plan->path, trainstate, &trainstate->last_known_pos, &plan->stopping_pos
  //     );
  //     TerminalUpdateStatus(terminal, "Time to stopping distance (ticks): %d", travel_time);

  //     TerminalLogPrint(
  //         terminal,
  //         "Distance from %s (last known position) to %s (dest): %d",
  //         trainstate->last_known_pos.node->name,
  //         plan->dest.node->name,
  //         get_distance_between(&plan->path, &trainstate->last_known_pos, &plan->dest)
  //     );

  //     // update time to stop
  //     plan->time_to_stop = Time(clock_server) + travel_time;
  //     if (next_sensor_index != -1) {
  //     } else {
  //       TerminalLogPrint(terminal, "No sensors left until destination.");
  //     }

  //     struct TrackNode *sensor_node = plan->path.nodes[next_sensor_index];
  //     struct TrackPosition sensor_pos = {.node = sensor_node, .offset = 0};
  //     int next_sensor_travel_time = get_travel_time_between(
  //         &plan->path, trainstate, &trainstate->last_known_pos, &sensor_pos
  //     );
  //     plan->next_sensor_eta = Time(clock_server) + next_sensor_travel_time;

  //     TerminalLogPrint(
  //         terminal,
  //         "Next sensor: %s, %d ticks away, %dmm away",
  //         sensor_node->name,
  //         next_sensor_travel_time,
  //         get_distance_between(&plan->path, &trainstate->last_known_pos, &sensor_pos)
  //     );

  //     // TODO: handle skipped sensors.
  //   }

  //   // TODO: maybe we should stop here instead of in the timer request?

  //   break;
  // }
  // // get next sensor and distance to next sensor
  // // get stopping distance (sensor and offset)
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

  struct TrackPosition src = {.node = track[45].reverse, .offset = 0};
  struct TrackPosition dest = {.node = &track[43], .offset = 0};

  TerminalLogPrint(terminal, "Finding shortest path....");
  TerminalLogPrint(terminal, "Creating route plan:");

  struct RoutePlan route = CreatePlan(train_planner, &src, &dest);

  TerminalLogPrint(
      terminal, "actual destination name: %s offset: %d", route.dest.node->name, route.dest.offset
  );

  if (route.path_found) {
    TerminalLogPrint(terminal, "Path found!");
  } else {
    TerminalLogPrint(terminal, "Path not found!");
  }

  for (unsigned int i = 0; i < route.paths_len; ++i) {
    struct SimplePath path = route.paths[i];
    TerminalLogPrint(
        terminal,
        "start: %d, end: %d, reverse: %d\r\n",
        path.start_index,
        path.end_index,
        path.reverse
    );

    for (int j = path.start_index; j >= path.end_index; --j) {
      switch (route.path.directions[j]) {
        case DIR_AHEAD:
          TerminalLogPrint(
              terminal,
              "%s distance: %d, AHEAD",
              route.path.nodes[j]->name,
              route.path.nodes[j]->edge[DIR_AHEAD].dist
          );
          break;
        case DIR_CURVED:
          TerminalLogPrint(
              terminal,
              "%s distance: %d, CURVED",
              route.path.nodes[j]->name,
              route.path.nodes[j]->edge[DIR_CURVED].dist
          );
          break;
        case DIR_REVERSE:
          TerminalLogPrint(terminal, "%s, REVERSE", route.path.nodes[j]->name);
          break;
      }
    }
  }

  struct TrackPosition train_pos = {
      .node = route.path.nodes[route.paths[route.paths_len - 1].end_index], .offset = 0
  };
  struct TrackPosition dest_pos = {
      .node = route.path.nodes[route.paths[route.paths_len - 1].end_index], .offset = 0
  };
  int dist_to_current_dest = get_distance_between(&route.path, &train_pos, &dest_pos);
  TerminalLogPrint(
      terminal,
      "distance between %s, offset: %d and %s offset: %d is %d",
      train_pos.node->name,
      train_pos.offset,
      dest_pos.node->name,
      dest_pos.offset,
      dist_to_current_dest
  );

  // TODO when we reverse, flip current_pos.
  // TODO set train as active if it hits sensor at start of predetermined positions
  // Create(TRAIN_TASK_PRIORITY, train_manager_tick_notifier);

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
        handle_tick(terminal, train_tid, clock_server, trains);
        Reply(tid, NULL, 0);
    }
  }
}

void NewTrain(int train) {}

// Executes the plan for the provided train and stops execution of
// any currently running plans
void ExecutePlan(struct RoutePlan plan) {}
