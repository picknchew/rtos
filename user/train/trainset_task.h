#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "trainset.h"

enum TrainRequestType {
  SET_SPEED,
  SET_TRACK,
  GET_SELECTED_TRACK,
  REVERSE_TRAIN,
  REVERSE_TRAIN_INSTANT,
  SET_SWITCH_DIR,
  IS_VALID_TRAIN,
  GET_SWITCH_STATE,
  OFF_LAST_SOLENOID_NOTIFY,
  REVERSE_TRAIN_NOTIFY,
  SENSOR_DATA_NOTIFY
};

extern const int TRAIN_TASK_PRIORITY;

struct TrainUpdateSensorDataRequest {
  bool *sensor_data;
};

struct TrainReverseNotifyRequest {
  int train;
  int speed;
};

struct TrainSetTrainSpeedRequest {
  int train;
  int speed;
};

struct TrainReverseRequest {
  int train;
};

struct TrainSetSwitchDirectionRequest {
  int switch_num;
  int dir;
};

struct TrainGetSwitchStateRequest {
  int switch_num;
};

struct TrainIsValidTrainRequest {
  int train;
};

struct TrainSetTrackRequest {
  char track;
};

struct TrainResponse {
  union {
    bool is_valid_train;
    enum SwitchDirection switch_state;
    enum Track selected_track;
  };
};

struct TrainRequest {
  enum TrainRequestType type;

  union {
    struct TrainUpdateSensorDataRequest update_sensor_data_req;
    struct TrainReverseNotifyRequest reverse_notify_req;
    struct TrainReverseRequest reverse_req;
    struct TrainSetTrainSpeedRequest set_train_speed_req;
    struct TrainSetSwitchDirectionRequest set_switch_dir_req;
    struct TrainIsValidTrainRequest is_valid_train_req;
    struct TrainGetSwitchStateRequest get_switch_state_req;
    struct TrainSetTrackRequest set_track_req;
  };
};

void train_task();
void train_reverse_task();
void train_off_solenoid_task();

void TrainUpdateSensorData(int tid, bool *sensor_data);
void TrainReverse(int tid, uint8_t train);
void TrainReverseInstant(int tid, uint8_t train);
void TrainSetSpeed(int tid, uint8_t train, uint8_t speed);
void TrainSetTrack(int tid, char track);
enum Track TrainGetSelectedTrack(int tid);
void TrainSetSwitchDir(int tid, int switch_num, int dir);
bool TrainIsValidTrain(int tid, uint8_t train);
enum SwitchDirection TrainGetSwitchState(int tid, uint8_t switch_num);
