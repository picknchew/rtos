#include "train_calibrator.h"

#include "selected_track.h"
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

// static const int loop_switches2[] = {10, 9, 16, 15,154};
// static int loop_switch_dirs2[] = {
//     TRAINSET_DIRECTION_CURVED,
//     TRAINSET_DIRECTION_CURVED,
//     TRAINSET_DIRECTION_STRAIGHT,
//     TRAINSET_DIRECTION_CURVED,
//     TRAINSET_DIRECTION_CURVED
// };

enum TrainCalibrationRequestType {
  CALIBRATOR_BEGIN_CALIBRATION,
  CALIBRATOR_SENSOR_UPDATE,
  CALIBRATOR_BEGIN_SHORTMOVE,
  CALIBRATOR_ACCELERATION_DISTANCE
};

struct TrainCalibrationBeginCalibrationRequest {
  int train;
  int speed;
};

struct TrainCalibrationBeginAccelerationDistanceRequest {
  int train;
  int speed;
};

struct TrainCalibrationBeginShortMoveRequest {
  int train;
  int speed;
  int delay;
};

struct TrainCalibrationUpdateSensorRequest {
  bool *sensors;
};

struct TrainCalibrationRequest {
  enum TrainCalibrationRequestType type;

  union {
    struct TrainCalibrationBeginCalibrationRequest begin_req;
    struct TrainCalibrationUpdateSensorRequest update_sensor_req;
    struct TrainCalibrationBeginShortMoveRequest short_move_req;
    struct TrainCalibrationBeginAccelerationDistanceRequest accd_req;
  };
};

static const int CALIBRATION_LOOPS = 10;

void train_calibrator_task() {
  RegisterAs("train_calib");

  int train_tid = WhoIs("train");
  int terminal_tid = WhoIs("terminal");
  int clock_server = WhoIs("clock_server");

  bool calibrating = false;

  int train = 0;
  int speed = 0;
  int delay = 0;

  int sensor = trainset_get_sensor_index("A4");
  // int accd_sensor1 = trainset_get_sensor_index("D4");
  // int accd_sensor2 = trainset_get_sensor_index("B6");
  int accd_sensor1 = trainset_get_sensor_index("B6");
  int accd_sensor2 = trainset_get_sensor_index("C12");
  bool began_measurement = false;
  bool accd_measurement = false;
  bool stop = false;
  bool update = false;
  bool shortmove = false;
  int short_move_start_sensor = -1;
  int short_move_end_sensor = -1;
  int loops = 0;
  struct TrackDistance info;

  int total_time = 0;
  int t1 = 0;
  int t2 = 0;
  int t_start = 0;
  int start = -1;

  int tid;
  struct TrainCalibrationRequest req;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case CALIBRATOR_BEGIN_CALIBRATION:
        stop = true;
        // calibration already running
        if (calibrating) {
          Reply(tid, NULL, 0);
          break;
        }

        calibrating = true;

        train = req.begin_req.train;
        speed = req.begin_req.speed;

        // TODO: switch to calibrate view

        // switch initialization - put train in a loop
        for (int i = 0; i < 8; i++) {
          TrainSetSwitchDir(train_tid, loop_switches[i], loop_switch_dirs[i]);
        }

        info = track_distance(track, track[2], track[2]);
        TerminalUpdateDistance(terminal_tid, info.begin, info.end, info.distance);

        TrainSetSpeed(train_tid, train, speed);
        Reply(tid, NULL, 0);
        break;
      case CALIBRATOR_ACCELERATION_DISTANCE:
        start = -1;
        stop = false;
        update = false;
        train = req.short_move_req.train;
        speed = req.short_move_req.speed;

        accd_measurement = true;

        t_start = Time(clock_server);
        TrainSetSpeed(train_tid, train, speed);
        Reply(tid, NULL, 0);
        break;

      case CALIBRATOR_SENSOR_UPDATE:
        bool *sensors = req.update_sensor_req.sensors;

        // only respond to sensor triggers
        if (calibrating) {
          if (sensors[sensor]) {
            if (began_measurement) {
              t2 = Time(clock_server);
              int tdelta = t2 - t1;

              // first loop ignore
              if (loops == 0) {
                total_time += tdelta;
              }

              TerminalUpdateVelocity(
                  terminal_tid, train, speed, tdelta, (info.distance * 100) / (tdelta)
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
              calibrating = false;
            }
          }
        }

        // if (shortmove) {
        //   int s = 0;
        //   for (int i = 0; i < 80; i++) {
        //     if (sensors[i]) {
        //       s = i;
        //       break;
        //     }
        //   }
        //   if (short_move_start_sensor == -1) {
        //     short_move_start_sensor = s;
        //   } else {
        //     short_move_end_sensor = s;
        //   }
        // }

        // if (accd_measurement) {
        //   if (sensors[accd_sensor1]) {
        //     t1 = Time(clock_server);
        //   } else if (sensors[accd_sensor2]) {
        //     t2 = Time(clock_server);
        //     int tdelta = t2 - t1;
        //     TerminalLogPrint(terminal_tid, "time between sensor triggers: %d", tdelta);
        //     TerminalLogPrint(
        //         terminal_tid,
        //         "Measured acceleration for train %d at speed %d, measured speed: %d, time to
        //         first " "sensor: %d", train, speed,
        //         // distance from B6 to C12.
        //         (351 * 1000) / (tdelta),
        //         t1 - t_start
        //     );
        //     accd_measurement = false;
        //     stop = true;
        //     TrainSetSpeed(train_tid, train, 0);
        //   }
        // }

        Reply(tid, NULL, 0);
        break;
    }
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
 * avg time = 2526.466667
 * distance = 4665
 * 1520
 */

void TrainCalibratorBeginCalibration(int tid, int train, int speed) {
  struct TrainCalibrationRequest req = {
      .type = CALIBRATOR_BEGIN_CALIBRATION, .begin_req = {.train = train, .speed = speed}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainCalibratorBeginShortMove(int tid, int train, int speed, int delay) {
  struct TrainCalibrationRequest req = {
      .type = CALIBRATOR_BEGIN_SHORTMOVE,
      .short_move_req = {.train = train, .speed = speed, .delay = delay}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainCalibratorBeginAccelerationDistance(int tid, int train, int speed) {
  struct TrainCalibrationRequest req = {
      .type = CALIBRATOR_ACCELERATION_DISTANCE, .short_move_req = {.train = train, .speed = speed}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainCalibratorUpdateSensors(int tid, bool *sensors) {
  struct TrainCalibrationRequest req = {
      .type = CALIBRATOR_SENSOR_UPDATE, .update_sensor_req = {.sensors = sensors}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}