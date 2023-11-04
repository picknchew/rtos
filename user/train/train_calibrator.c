#include "train_calibrator.h"

#include "syscall.h"
#include "trackdata/distance.h"
#include "trackdata/track_data.h"
#include "trainset.h"
#include "trainset_task.h"
#include "user/server/clock_server.h"
#include "user/server/name_server.h"
#include "user/terminal/terminal_task.h"

static const int loop_switches[] = {14, 13, 10, 9, 8, 17, 16, 15};
static int loop_switch_dirs[] = {
    TRAINSET_DIRECTION_CURVED,
    TRAINSET_DIRECTION_STRAIGHT,
    TRAINSET_DIRECTION_STRAIGHT,
    TRAINSET_DIRECTION_CURVED,
    TRAINSET_DIRECTION_CURVED,
    TRAINSET_DIRECTION_STRAIGHT,
    TRAINSET_DIRECTION_STRAIGHT,
    TRAINSET_DIRECTION_CURVED};

static unsigned int get_sensor_index(char ch, int offset) {
  return (ch - 'A') * 16 + offset - 1;
}

static struct TrackNode track[TRACK_MAX];

void train_calibrator_task() {
  RegisterAs("train_calib");

  int train_tid = WhoIs("train");
  int terminal_tid = WhoIs("terminal");
  int clock_server = WhoIs("clock_server");

  // switch initilization - put train in a loop
  for (int i = 0; i < 8; i++) {
    TrainSetSwitchDir(train_tid, loop_switches[i], loop_switch_dirs[i]);
  }

  tracka_init(track);

  struct TrackDistance info = track_distance(track, track[2]);
  TerminalUpdateDistance(terminal_tid, info.begin, info.end, info.distance);

  bool *sensors;

  int train = 77;
  int speed = 7;

  TrainSetSpeed(train_tid, train, speed);

  // sensor A3
  int sensor = get_sensor_index('A', 3);

  bool prev_sensor_state = false;

  int t = 0;
  int tid;
  for (int i = 0; i < 20; ++i) {
    int t1 = 0;
    int t2 = 0;

    while (true) {
      Receive(&tid, (char *) &sensors, sizeof(sensors));
      t1 = Time(clock_server);
      Reply(tid, NULL, 0);

      if (!prev_sensor_state && sensors[sensor]) {
        break;
      }

      prev_sensor_state = sensors[sensor];
    }

    prev_sensor_state = true;

    while (true) {
      Receive(&tid, (char *) &sensors, sizeof(sensors));
      t2 = Time(clock_server);
      Reply(tid, NULL, 0);

      if (!prev_sensor_state && sensors[sensor]) {
        break;
      }

      prev_sensor_state = sensors[sensor];
    }

    int t_delta = t2 - t1;

    // skip first loop to let train accelerate to constant speed
    if (i >= 2) {
      t += t_delta;
    }

    TerminalUpdateStatus(terminal_tid, "looped: vel %d, time %d", speed, t_delta);
    TerminalUpdateVelocity(terminal_tid, train, speed, t_delta, info.distance / t_delta);
  }

  Exit();
}

/**
 * train 77 at velocity 7
 * 2529
 * 2530
 * 2522
 * 2529
 * 2523
 * 2530
 * 2524
 * 2530
 * 2523
 * 2529
 * 2524
 * 2530
 * 2522
 * 2529
 * 2523
*/

void TrainCalibratorUpdateSensors(int tid, bool *sensors) {
  Send(tid, (const char *) &sensors, sizeof(sensors), NULL, 0);
}
