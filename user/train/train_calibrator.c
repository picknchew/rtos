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
    TRAINSET_DIRECTION_CURVED
};

static unsigned int get_sensor_index(char ch, int offset) {
  return (ch - 'A') * 16 + offset - 1;
}


enum TrainCalibrationRequestType { CALIBRATOR_BEGIN_CALIBRATION, CALIBRATOR_SENSOR_UPDATE };

struct TrainCalibrationBeginCalibrationRequest {
  int train;
  int speed;
};

struct TrainCalibrationUpdateSensorRequest {
  bool *sensors;
};

struct TrainCalibrationRequest {
  enum TrainCalibrationRequestType type;

  union {
    struct TrainCalibrationBeginCalibrationRequest begin_req;
    struct TrainCalibrationUpdateSensorRequest update_sensor_req;
  };
};

static const int CALIBRATION_LOOPS = 10;
static struct TrackNode track[TRACK_MAX];

void train_calibrator_task() {
  RegisterAs("train_calib");

  int train_tid = WhoIs("train");
  int terminal_tid = WhoIs("terminal");
  int clock_server = WhoIs("clock_server");

  tracka_init(track);

  bool calibrating = true;

  int train = 0;
  int speed = 0;

  int sensor = trainset_get_sensor_index("A3");
  // false = not triggered
  bool prev_sensor_state = false;
  bool began_measurement = false;
  int loops = 0;

  struct TrackDistance info = track_distance(track, track[2]);
  TerminalUpdateDistance(terminal_tid, info.begin, info.end, info.distance);

  int total_time = 0;
  int t1 = 0;
  int t2 = 0;

  int tid;
  struct TrainCalibrationRequest req;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case CALIBRATOR_BEGIN_CALIBRATION:
        // calibration already running
        if (calibrating) {
          Reply(tid, 0, NULL);
          break;
        }

        train = req.begin_req.train;
        speed = req.begin_req.speed;

        // TODO: switch to calibrate view

        // switch initialization - put train in a loop
        for (int i = 0; i < 8; i++) {
          TrainSetSwitchDir(train_tid, loop_switches[i], loop_switch_dirs[i]);
        }

        TrainSetSpeed(train_tid, train, speed);
        Reply(tid, 0, NULL);
        break;
      case CALIBRATOR_SENSOR_UPDATE:
        if (!calibrating) {
          Reply(tid, 0, NULL);
          // do nothing
          break;
        }

        bool *sensors = req.update_sensor_req.sensors;

        // debounce sensor triggers
        if (prev_sensor_state != sensors[sensor] && sensors[sensor]) {
          if (began_measurement) {
            t2 = Time(clock_server);
            int tdelta = t2 - t1;

            // first loop ignore
            if (loops == 0) {
              total_time += tdelta;
            }

            TerminalUpdateVelocity(
                terminal_tid, train, speed, tdelta, (info.distance * 100) / (tdelta * 100)
            );

            // TODO: print delta
            ++loops;

            // begin measuring next loop
            t1 = t2;
          } else {
            t1 = Time(clock_server);
            began_measurement = true;
          }

          if (loops == CALIBRATION_LOOPS + 1) {
            // TODO: print final results

            // TODO: switch back to shell view

            // reset values
            total_time = 0;
            loops = 0;
            began_measurement = false;
            prev_sensor_state = false;
            calibrating = false;
          }
        }

        prev_sensor_state = sensors[sensor];
        Reply(tid, 0, NULL);
        break;
    }
  }

  Exit();
}

// potential stopping distance
// d = v^2 / (2 * mu * g)
// mu = coefficient of friction

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
 * avg time = 2526.466667
 */

void TrainCalibratorBeginCalibration(int tid, int train, int speed) {
  struct TrainCalibrationRequest req = {
      .type = CALIBRATOR_BEGIN_CALIBRATION, .begin_req = {.train = train, .speed = speed}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainCalibratorUpdateSensors(int tid, bool *sensors) {
  struct TrainCalibrationRequest req = {
      .type = CALIBRATOR_SENSOR_UPDATE, .update_sensor_req = {.sensors = sensors}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
