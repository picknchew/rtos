#pragma once

#include <stdarg.h>
#include <stdint.h>

#include "terminal.h"
#include "user/train/trainset.h"

enum TerminalRequestType {
  UPDATE_TRAIN_SPEED,
  UPDATE_SENSORS,
  UPDATE_STATUS,
  UPDATE_IDLE,
  UPDATE_SWITCH_STATE,
  UPDATE_MAX_SENSOR_DURATION,
  UPDATE_COMMAND,
  TERMINAL_TIME_NOTIFY,
  TERMINAL_DISTANCE,
  TERMINAL_TIME_LOOP
};

// struct VelocityMeasurementInfo{
//   int train_tid;
//   struct Terminal *terminal;
// };

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


struct TerminalUpdateDistanceRequest{
  const char * begin;
  const char * end;
  int distance;
};

struct TerminalUpdateVelocityRequest{
  int train_num;
  int train_speed;
  int loop_time;
  int train_velocity;
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
    struct TerminalUpdateDistanceRequest update_distance_req;
    struct TerminalUpdateVelocityRequest update_velocity_req;
    
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
void TerminalUpdateDistance(int tid, const char *begin, const char *end, int distance);
void TerminalUpdateVelocity(int tid, int train_num, int train_speed, int loop_time, int train_velocity);
void terminal_task();
