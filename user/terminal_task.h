#include "../train/terminal.h"

enum TerminalRequestType {
  UPDATE_TRAIN_SPEEDS,
  UPDATE_SENSORS,
  UPDATE_STATUS,
  UPDATE_SWITCH_STATES,
  UPDATE_MAX_SENSOR_DURATION,
  TERMINAL_TIME_NOTIFY,
  TERMINAL_KEY_PRESS_NOTIFY
};

struct TerminalUpdateTrainSpeedsRequest {
  uint8_t *train_speeds;
};

struct TerminalUpdateSensorsRequest {
  bool *sensors;
  size_t sensors_len;
};

struct TerminalUpdateStatusRequest {
  char *status;
};

struct TerminalUpdateMaxSensorDurationRequest {
  unsigned int duration;
};

struct TerminalRequest {
  enum TerminalRequestType type;

  union {
    char ch;
    uint64_t time;

    struct TerminalUpdateTrainSpeedsRequest update_train_speeds_req;
    struct TerminalUpdateSensorsRequest update_sensors_req;
    struct TerminalUpdateStatusRequest update_status_req;
    struct TerminalUpdateMaxSensorDurationRequest update_max_sensor_duration_req;
  };
};

void TerminalUpdateTrainSpeeds(int tid, uint8_t *train_speeds);
void TerminalUpdateSensors(int tid, bool *sensors, size_t sensors_len);
void TerminalUpdateStatus(int tid, char *status);
void TerminalUpdateSwitchStates(int tid);
void TerminalUpdateMaxSensorDuration(int tid, unsigned int duration);

int terminal_task();
