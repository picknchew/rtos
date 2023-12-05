#include "trainset.h"

#include <stdbool.h>

#include "selected_track.h"
#include "syscall.h"
#include "track_reservations.h"
#include "train_dispatcher.h"
#include "trainset_calib_data.h"
#include "trainset_task.h"
#include "uart.h"
#include "user/server/io_server.h"
#include "user/terminal/terminal_task.h"
#include "util.h"

static const int BAUD_RATE = 2400;

// delay to switch off last solenoid in ticks (150ms)
const unsigned int DELAY_OFF_LAST_SOLENOID = 15;
// delay between braking and reversing (2s) in ticks.
const unsigned int DELAY_REVERSE = 200;

static const unsigned char CMD_SENSOR_RESET_MODE = 0xC0;

// offset to add to speed to turn on function on train (headlights)
static const int SPEED_OFFSET_FUNCTION = 16;
const int SPEED_REVERSE_DIRECTION = 15;

const int TRAINSET_TRAINS[] = {1, 2, 24, 47, 54, 58, 77, 78};

struct TrackNode track[TRACK_MAX];

static enum Track selected_track = TRACK_B;

int marklin_tx_server;

int trainset_get_train_index(uint8_t train) {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    if (train == TRAINSET_TRAINS[i]) {
      return i;
    }
  }

  return 0;
}

int trainset_get_sensor_index(char *sensor) {
  char bucket = sensor[0];
  int offset = atoi(sensor + 1);

  return (bucket - 'A') * 16 + offset - 1;
}

void trainset_init(struct Trainset *trainset, int train_dispatcher_tid) {
  trainset_set_track(trainset, 'A');
  // ! initial zones for trains init
  trainset_calib_data_init();

  trainset->last_track_switch_time = 0;

  trainset->train_dispatcher = train_dispatcher_tid;
  for (unsigned int i = 0; i < TRAINSET_NUM_MAX_SWITCHES; ++i) {
    trainset->switch_states[i] = DIRECTION_UNKNOWN;
  }

  for (unsigned int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    trainset->train_speeds[i] = 0;
  }

  uart_config_and_enable(UART_MARKLIN, BAUD_RATE, true, false, true);

  unsigned char cmd[] = {CMD_SENSOR_RESET_MODE};
  DispatchTrainCommand(trainset->train_dispatcher, cmd, 1);
}

static void send_command(struct Trainset *trainset, unsigned char arg1, unsigned char arg2) {
  unsigned char data[] = {arg1, arg2};
  DispatchTrainCommand(trainset->train_dispatcher, data, 2);
}

void trainset_set_train_speed(
    struct Trainset *trainset,
    int terminal_tid,
    uint8_t train,
    uint8_t speed
) {
  int train_index = trainset_get_train_index(train);

  if (speed == SPEED_REVERSE_DIRECTION) {
    // reverse direction stops the train
    trainset->train_speeds[train_index] = 0;
  } else {
    trainset->train_speeds[train_index] = speed;
    TerminalUpdateTrainSpeed(terminal_tid, train_index, speed);
  }

  // add 16 to speed for auxiliary function
  send_command(trainset, speed + SPEED_OFFSET_FUNCTION, train);
}

bool trainset_is_valid_train(uint8_t train) {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    if (train == TRAINSET_TRAINS[i]) {
      return true;
    }
  }

  return false;
}

uint8_t trainset_get_speed(struct Trainset *trainset, uint8_t train) {
  return trainset->train_speeds[trainset_get_train_index(train)];
}

void trainset_train_reverse(struct Trainset *trainset, int terminal_tid, uint8_t train) {
  int speed = trainset_get_speed(trainset, train);
  trainset_set_train_speed(trainset, terminal_tid, train, 0);

  int reverse_task = Create(TRAIN_TASK_PRIORITY, train_reverse_task);
  struct TrainReverseNotifyRequest req = {.train = train, .speed = speed};
  Send(reverse_task, (const char *) &req, sizeof(req), NULL, 0);
}

void trainset_set_switch_direction(
    struct Trainset *trainset,
    int terminal_tid,
    int switch_number,
    int direction,
    uint64_t time
) {
  send_command(trainset, direction, switch_number);

  enum SwitchDirection switch_direction =
      direction == TRAINSET_DIRECTION_CURVED ? DIRECTION_CURVED : DIRECTION_STRAIGHT;

  trainset->last_track_switch_time = time;
  trainset->switch_states[switch_number] = switch_direction;
  TerminalUpdateSwitchState(terminal_tid, switch_number, switch_direction);

  // switch off solenoid after a period.
  Create(TRAIN_TASK_PRIORITY, train_off_solenoid_task);
}

void trainset_set_track(struct Trainset *trainset, char track_type) {
  switch (track_type) {
    case 'A':
      tracka_init(track);
      zones_a_init();
      selected_track = TRACK_A;
      break;
    case 'B':
      trackb_init(track);
      zones_b_init();
      selected_track = TRACK_B;
      break;
    default:
      break;
  }
}

enum Track trainset_get_track() {
  return selected_track;
}

void trainset_update_sensor_data(struct Trainset *trainset, bool *sensor_data) {
  trainset->sensors_occupied = sensor_data;
}

bool *trainset_get_sensor_data(struct Trainset *trainset) {
  return trainset->sensors_occupied;
}

enum SwitchDirection trainset_get_switch_state(struct Trainset *trainset, uint8_t switch_number) {
  return trainset->switch_states[switch_number];
}
