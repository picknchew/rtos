#include "trainset_task.h"

#include <stdbool.h>
#include <stdint.h>

#include "syscall.h"
#include "train_calibrator.h"
#include "train_dispatcher.h"
#include "trainset.h"
#include "user/server/clock_server.h"
#include "user/server/io_server.h"
#include "user/server/name_server.h"
#include "user/terminal/terminal_task.h"

const unsigned char CMD_OFF_LAST_SOLENOID[] = {0x20};

const int TRAIN_TASK_PRIORITY = 4;

void train_reverse_task() {
  int train_controller = WhoIs("train");
  int clock_server = WhoIs("clock_server");

  int tid;
  struct TrainReverseNotifyRequest reverse_notify_req;
  Receive(&tid, (char *) &reverse_notify_req, sizeof(reverse_notify_req));
  Reply(tid, NULL, 0);

  Delay(clock_server, DELAY_REVERSE);
  struct TrainRequest req = {
      .type = REVERSE_TRAIN_NOTIFY, .reverse_notify_req = reverse_notify_req
  };
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
  int marklin_tx = WhoIs("marklin_io_tx");
  int terminal = WhoIs("terminal");

  struct Trainset trainset;
  trainset_init(&trainset, marklin_tx);

  Create(TRAIN_TASK_PRIORITY, train_calibrator_task);

  int tid;
  struct TrainRequest req;
  struct TrainResponse res;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case SET_SPEED:
        trainset_set_train_speed(
            &trainset, terminal, req.set_train_speed_req.train, req.set_train_speed_req.speed
        );
        Reply(tid, NULL, 0);
        break;
      case SET_SWITCH_DIR:
        Reply(tid, NULL, 0);
        trainset_set_switch_direction(
            &trainset,
            terminal,
            req.set_switch_dir_req.switch_num,
            req.set_switch_dir_req.dir,
            Time(clock_server)
        );
        break;
      case REVERSE_TRAIN:
        Reply(tid, NULL, 0);
        TerminalUpdateStatus(terminal, "Reversing train..");
        trainset_train_reverse(&trainset, terminal, req.reverse_req.train);
        break;
      case REVERSE_TRAIN_INSTANT:
        trainset_set_train_speed(
            &trainset, terminal, req.reverse_req.train, SPEED_REVERSE_DIRECTION
        );
        Reply(tid, NULL, 0);
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
      case GET_SELECTED_TRACK:
        res.selected_track = trainset_get_track();
        Reply(tid, (const char *) &res, sizeof(res));
        break;
      case SET_TRACK:
        Reply(tid, NULL, 0);
        trainset_set_track(&trainset, req.set_track_req.track);
        break;
      case SENSOR_DATA_NOTIFY:
        trainset_update_sensor_data(&trainset, req.update_sensor_data_req.sensor_data);
        Reply(tid, NULL, 0);
        break;
      case REVERSE_TRAIN_NOTIFY:
        trainset_set_train_speed(
            &trainset, terminal, req.reverse_notify_req.train, SPEED_REVERSE_DIRECTION
        );
        trainset_set_train_speed(
            &trainset, terminal, req.reverse_notify_req.train, req.reverse_notify_req.speed
        );
        Reply(tid, NULL, 0);
        break;
      case OFF_LAST_SOLENOID_NOTIFY:
        Reply(tid, NULL, 0);
        handle_off_last_solenoid(&trainset, Time(clock_server));
        break;
    }
  }

  Exit();
}

void TrainReverse(int tid, uint8_t train) {
  struct TrainRequest req = {.type = REVERSE_TRAIN, .reverse_req = {.train = train}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainReverseInstant(int tid, uint8_t train) {
  struct TrainRequest req = {.type = REVERSE_TRAIN_INSTANT, .reverse_req = {.train = train}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainSetTrack(int tid, char track) {
  struct TrainRequest req = {.type = SET_TRACK, .set_track_req = {.track = track}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

enum Track TrainGetSelectedTrack(int tid) {
  struct TrainResponse res;
  struct TrainRequest req = {.type = GET_SELECTED_TRACK};
  Send(tid, (const char *) &req, sizeof(req), (char *) &res, sizeof(res));

  return res.selected_track;
}

void TrainSetSpeed(int tid, uint8_t train, uint8_t speed) {
  struct TrainRequest req = {
      .type = SET_SPEED, .set_train_speed_req = {.train = train, .speed = speed}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TrainSetSwitchDir(int tid, int switch_num, int dir) {
  struct TrainRequest req = {
      .type = SET_SWITCH_DIR, .set_switch_dir_req = {.switch_num = switch_num, .dir = dir}
  };
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
      .type = GET_SWITCH_STATE, .get_switch_state_req = {.switch_num = switch_num}
  };
  Send(tid, (const char *) &req, sizeof(req), (char *) &res, sizeof(res));

  return res.switch_state;
}

void TrainUpdateSensorData(int tid, bool *sensor_data) {
  struct TrainRequest req = {
      .type = SENSOR_DATA_NOTIFY, .update_sensor_data_req = {.sensor_data = sensor_data}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

// TODO:
// - route test to C5.
// - override routes
// -
