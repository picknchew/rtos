#include "train_sensor_notifier.h"

#include "syscall.h"
#include "train_calibrator.h"
#include "train_dispatcher.h"
#include "train_manager.h"
#include "train_router.h"
#include "trainset.h"
#include "trainset_task.h"
#include "user/server/clock_server.h"
#include "user/server/io_server.h"
#include "user/server/name_server.h"
#include "user/terminal/terminal_task.h"

static const unsigned char CMD_READ_ALL_SENSORS[] = {0x80 + TRAINSET_NUM_FEEDBACK_MODULES};

static bool process_sensor_data(char *raw_sensor_data, bool *out_sensor_data) {
  bool diff = false;

  // each feedback module has 2 numbers (contacts 1 to 8) and (contacts 9 to 16)
  for (int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * 2; ++i) {
    char ch = raw_sensor_data[i];

    // 1 byte (8 bits) and each char represents 8 sensors
    // the most significant bit represents the lowest sensor
    for (int j = 0; j < 8; ++j) {
      // take last bit each time
      bool sensor_triggered = (ch >> (7 - j)) & 1;

      if (out_sensor_data[i * 8 + j] != sensor_triggered) {
        out_sensor_data[i * 8 + j] = sensor_triggered;
        diff = true;
      }
    }
  }

  return diff;
}

void train_sensor_notifier_task() {
  int marklin_rx = WhoIs("marklin_io_rx");
  int marklin_tx = WhoIs("marklin_io_tx");
  int clock_server = WhoIs("clock_server");

  // tasks to notify
  int train = WhoIs("train");
  int terminal = WhoIs("terminal");
  int train_calib = WhoIs("train_calib");
  // int train_router = WhoIs("train_router");
  int train_manager = WhoIs("train_manager");

  bool sensors[TRAINSET_SENSORS_LEN] = {0};
  char raw_sensor_data[TRAINSET_NUM_FEEDBACK_MODULES * 2] = {0};
  uint64_t max_time_taken = 0;

  while (true) {
    uint64_t start_time = Time(clock_server);

    DispatchTrainCommand(marklin_tx, CMD_READ_ALL_SENSORS, 1);
    for (int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * 2; ++i) {
      raw_sensor_data[i] = Getc(marklin_rx);
    }
    NotifyMarklinRead(marklin_tx);

    uint64_t end_time = Time(clock_server);
    uint64_t time_taken = end_time - start_time;

    if (process_sensor_data(raw_sensor_data, sensors)) {
      TrainCalibratorUpdateSensors(train_calib, sensors);
      // TrainRouterUpdateSensors(train_router, sensors);
      TrainManagerUpdateSensors(train_manager, sensors);
      TerminalUpdateSensors(terminal, sensors, TRAINSET_SENSORS_LEN);
      TrainUpdateSensorData(train, sensors);
    }

    if (max_time_taken < time_taken) {
      max_time_taken = time_taken;
      TerminalUpdateMaxSensorDuration(terminal, time_taken);
    }
  }

  Exit();
}
