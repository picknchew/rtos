#pragma once

#include <stdarg.h>
#include <stdint.h>

#include "train/terminal.h"
#include "train/trainset.h"

enum TerminalRequestType {
  UPDATE_TRAIN_SPEED,
  UPDATE_SENSORS,
  UPDATE_STATUS,
  UPDATE_IDLE,
  UPDATE_SWITCH_STATE,
  UPDATE_MAX_SENSOR_DURATION,
  UPDATE_COMMAND,
  TERMINAL_TIME_NOTIFY
};

struct TerminalUpdateTrainSpeedRequest {
  int train_index;
  uint8_t train_speed;
};

struct TerminalUpdateSensorsRequest {
  bool *sensors;
  size_t sensors_len;
};

struct TerminalUpdateIdleRequest {
  uint64_t idle;
  int idle_pct;
};

struct TerminalUpdateStatusRequest {
  char *fmt;
  va_list va;
};

struct TerminalUpdateSwitchRequest {
  int switch_num;
  enum SwitchDirection dir;
};

struct TerminalUpdateMaxSensorDurationRequest {
  unsigned int duration;
};

struct TerminalUpdateCommandRequest {
  char *command;
  size_t len;
};

struct TerminalRequest {
  enum TerminalRequestType type;

  union {
    char ch;
    uint64_t time;

    struct TerminalUpdateTrainSpeedRequest update_train_speed_req;
    struct TerminalUpdateSensorsRequest update_sensors_req;
    struct TerminalUpdateStatusRequest update_status_req;
    struct TerminalUpdateMaxSensorDurationRequest update_max_sensor_duration_req;
    struct TerminalUpdateIdleRequest update_idle_req;
    struct TerminalUpdateSwitchRequest update_switch_state_req;
    struct TerminalUpdateCommandRequest update_command_req;
  };
};

extern const int TERMINAL_TASK_PRIORITY;

void TerminalUpdateTrainSpeed(int tid, int train_index, uint8_t train_speed);
void TerminalUpdateSensors(int tid, bool *sensors, size_t sensors_len);
void TerminalUpdateStatus(int tid, char *status, ...);
void TerminalUpdateSwitchState(int tid, int switch_num, enum SwitchDirection dir);
void TerminalUpdateMaxSensorDuration(int tid, unsigned int duration);
void TerminalUpdateIdle(int tid, uint64_t idle, int idle_pct);
void TerminalUpdateCommand(int tid, char *command, size_t len);

void terminal_task();
