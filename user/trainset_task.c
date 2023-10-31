#include "trainset_task.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../syscall.h"
#include "../train/train_dispatcher.h"
#include "../train/trainset.h"
#include "../uart.h"
#include "../user/terminal_task.h"
#include "clock_server.h"
#include "io_server.h"
#include "name_server.h"

const unsigned char CMD_OFF_LAST_SOLENOID[] = {0x20};
const unsigned char CMD_READ_ALL_SENSORS[] = {0x80 + TRAINSET_NUM_FEEDBACK_MODULES};

const int TRAIN_TASK_PRIORITY = 3;

void train_sensor_update_task() {
  int train = MyParentTid();
  int marklin_io_tx = WhoIs("marklin_io_tx");
  int marklin_rx = WhoIs("marklin_io_rx");
  int clock_server = WhoIs("clock_server");
  int terminal = WhoIs("terminal");

  struct TrainRequest req = {.type = SENSOR_DATA_NOTIFY, .update_sensor_data_req = {}};

  while (true) {
    // Delay(clock_server, 10);

    uint64_t start_time = Time(clock_server);

    DispatchTrainCommand(marklin_io_tx, CMD_READ_ALL_SENSORS, 1);

    TerminalUpdateStatus(terminal, "begin read sensor");
    for (int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * 2; ++i) {
      req.update_sensor_data_req.raw_sensor_data[i] = Getc(marklin_rx);
      TerminalUpdateStatus(
          terminal, "latest sensor byte read: %d/%d", i, TRAINSET_NUM_FEEDBACK_MODULES * 2 - 1);
    }
    TerminalUpdateStatus(terminal, "completed reading set");
    NotifyMarklinRead(marklin_io_tx);
    TerminalUpdateStatus(terminal, "notified marklin done read");

    uint64_t end_time = Time(clock_server);
    // get in microseconds
    req.update_sensor_data_req.time_taken = end_time - start_time;

    Send(train, (const char *) &req, sizeof(req), NULL, 0);
    TerminalUpdateStatus(terminal, "sent info back to main train task");
  }

  Exit();
}

void train_reverse_task() {
  int train_controller = WhoIs("train");
  int clock_server = WhoIs("clock_server");

  int tid;
  struct TrainReverseNotifyRequest reverse_notify_req;
  Receive(&tid, (char *) &reverse_notify_req, sizeof(reverse_notify_req));
  Reply(tid, NULL, 0);

  Delay(clock_server, DELAY_REVERSE);
  struct TrainRequest req = {
      .type = REVERSE_TRAIN_NOTIFY, .reverse_notify_req = reverse_notify_req};
  Send(train_controller, (const char *) &req, sizeof(req), NULL, 0);
  Exit();
}

void train_off_solenoid_task() {
  int train_controller = WhoIs("train");
  int clock_server = WhoIs("clock_server");

  struct TrainRequest req = {.type = OFF_LAST_SOLENOID_NOTIFY};
  Delay(clock_server, DELAY_OFF_LAST_SOLENOID);
  Send(train_controller, (const char *) &req, sizeof(req), NULL, 0);
  Exit();
}

static void handle_off_last_solenoid(struct Trainset *trainset, uint64_t current_time) {
  // multiple switches in a short duration can cause the solenoid to be switched off many times
  // when it is already off.
  if (current_time - trainset->last_track_switch_time >= DELAY_OFF_LAST_SOLENOID) {
    DispatchTrainCommand(trainset->train_dispatcher, CMD_OFF_LAST_SOLENOID, 1);
  }
}

void train_task() {
  RegisterAs("train");

  int clock_server = WhoIs("clock_server");
  int marklin_io_tx = WhoIs("marklin_io_tx");
  int marklin_rx = WhoIs("marklin_io_rx");

  struct Trainset trainset;
  trainset_init(&trainset, marklin_io_tx);

  int terminal = WhoIs("terminal");

  // create sensor update task
  Create(TRAIN_TASK_PRIORITY, train_sensor_update_task);

  int tid;
  struct TrainRequest req;
  struct TrainResponse res;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case SET_SPEED:
        Reply(tid, NULL, 0);
        TerminalUpdateStatus(terminal, "trainset: set speed");
        trainset_set_train_speed(
            &trainset, terminal, req.set_train_speed_req.train, req.set_train_speed_req.speed);
        TerminalUpdateStatus(terminal, "trainset: after set speed");
        break;
      case SET_SWITCH_DIR:
        Reply(tid, NULL, 0);
        trainset_set_switch_direction(
            &trainset,
            terminal,
            req.set_switch_dir_req.switch_num,
            req.set_switch_dir_req.dir,
            Time(clock_server));
        break;
      case REVERSE_TRAIN:
        Reply(tid, NULL, 0);
        trainset_train_reverse(&trainset, terminal, req.reverse_req.train);
        break;
      case IS_VALID_TRAIN:
        res.is_valid_train = trainset_is_valid_train(req.is_valid_train_req.train);
        Reply(tid, (const char *) &res, sizeof(res));
        break;
      case GET_SWITCH_STATE:
        res.switch_state =
            trainset_get_switch_state(&trainset, req.get_switch_state_req.switch_num);
        Reply(tid, (const char *) &res, sizeof(res));
        break;
      case SENSOR_DATA_NOTIFY:
        TerminalUpdateStatus(terminal, "trainset: sensor data notify");
        trainset_process_sensor_data(&trainset, req.update_sensor_data_req.raw_sensor_data);

        TerminalUpdateStatus(terminal, "trainset: processed sensor data");

        TerminalUpdateSensors(
            terminal,
            trainset_get_sensor_data(&trainset),
            TRAINSET_NUM_FEEDBACK_MODULES * TRAINSET_NUM_SENSORS_PER_MODULE);
        TerminalUpdateStatus(terminal, "update sensor data");

        if (trainset.max_read_sensor_query_time < req.update_sensor_data_req.time_taken) {
          trainset.max_read_sensor_query_time = req.update_sensor_data_req.time_taken;
          // TerminalUpdateMaxSensorDuration(terminal, trainset.max_read_sensor_query_time);
        }

        TerminalUpdateStatus(terminal, "replied to sensor notifier");

        Reply(tid, NULL, 0);
        break;
      case READ_SENSORS: {
        // char raw_sensor_data[TRAINSET_NUM_FEEDBACK_MODULES * 2] = {0};
        // uint64_t start_time = Time(clock_server);

        // DispatchTrainCommand(marklin_io_tx, CMD_READ_ALL_SENSORS, 1);

        // for (int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * 2; ++i) {
        //   raw_sensor_data[i] = Getc(marklin_rx);
        // }
        // NotifyMarklinRead(marklin_io_tx);

        // uint64_t end_time = Time(clock_server);
        // // get in microseconds
        // uint64_t time_taken = end_time - start_time;

        // trainset_process_sensor_data(&trainset, raw_sensor_data);

        // TerminalUpdateSensors(
        //     terminal,
        //     trainset_get_sensor_data(&trainset),
        //     TRAINSET_NUM_FEEDBACK_MODULES * TRAINSET_NUM_SENSORS_PER_MODULE);

        // if (trainset.max_read_sensor_query_time < time_taken) {
        //   trainset.max_read_sensor_query_time = time_taken;
        //   TerminalUpdateMaxSensorDuration(terminal, time_taken);
        // }
        Reply(tid, NULL, 0);
        break;
      }
      case REVERSE_TRAIN_NOTIFY:
        TerminalUpdateStatus(terminal, "Reversing train..");
        trainset_set_train_speed(
            &trainset, terminal, req.reverse_notify_req.train, SPEED_REVERSE_DIRECTION);
        // Delay(clock_server, 10);
        trainset_set_train_speed(
            &trainset, terminal, req.reverse_notify_req.train, req.reverse_notify_req.speed);
        Reply(tid, NULL, 0);
        break;
      case OFF_LAST_SOLENOID_NOTIFY:
        Reply(tid, NULL, 0);
        handle_off_last_solenoid(&trainset, Time(clock_server));
    }
  }

  Exit();
}

void TrainReverse(int tid, uint8_t train) {
  struct TrainRequest req = {.type = REVERSE_TRAIN, .reverse_req = {.train = train}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainSetSpeed(int tid, uint8_t train, uint8_t speed) {
  struct TrainRequest req = {
      .type = SET_SPEED, .set_train_speed_req = {.train = train, .speed = speed}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainSetSwitchDir(int tid, int switch_num, int dir) {
  struct TrainRequest req = {
      .type = SET_SWITCH_DIR, .set_switch_dir_req = {.switch_num = switch_num, .dir = dir}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainReadSensors(int tid) {
  struct TrainRequest req = {.type = READ_SENSORS};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

bool TrainIsValidTrain(int tid, uint8_t train) {
  struct TrainResponse res;
  struct TrainRequest req = {.type = IS_VALID_TRAIN, .is_valid_train_req = {.train = train}};
  Send(tid, (const char *) &req, sizeof(req), (char *) &res, sizeof(res));

  return res.is_valid_train;
}

enum SwitchDirection TrainGetSwitchState(int tid, uint8_t switch_num) {
  struct TrainResponse res;
  struct TrainRequest req = {
      .type = GET_SWITCH_STATE, .get_switch_state_req = {.switch_num = switch_num}};
  Send(tid, (const char *) &req, sizeof(req), (char *) &res, sizeof(res));

  return res.switch_state;
}
