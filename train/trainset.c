#include "trainset.h"

#include <stdbool.h>

#include "../syscall.h"
#include "../uart.h"
#include "../user/io_server.h"
#include "../user/terminal_task.h"
#include "../user/trainset_task.h"
#include "train_dispatcher.h"

static const int LINE_TRAINSET = 2;
static const int BAUD_RATE = 2400;

// delay to switch off last solenoid in ticks (150ms)
const unsigned int DELAY_OFF_LAST_SOLENOID = 15000;
// delay between braking and reversing (2s) in ticks.
const unsigned int DELAY_REVERSE = 200;

const int CMD_OFF_LAST_SOLENOID = 0x20;
const int CMD_READ_ALL_SENSORS = 0x80 + TRAINSET_NUM_FEEDBACK_MODULES;
static const int CMD_SENSOR_RESET_MODE = 0xC0;

// offset to add to speed to turn on function on train (headlights)
static const int SPEED_OFFSET_FUNCTION = 16;
const int SPEED_REVERSE_DIRECTION = 15;

const int TRAINSET_DIRECTION_STRAIGHT = 0x21;
const int TRAINSET_DIRECTION_CURVED = 0x22;

const int TRAINSET_TRAINS[] = {1, 2, 24, 47, 54, 58, 78};

int marklin_rx_server;
int marklin_tx_server;

static int get_train_index(uint8_t train) {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    if (train == TRAINSET_TRAINS[i]) {
      return i;
    }
  }

  return 0;
}

void trainset_init(struct Trainset *trainset, int marklin_rx_tid, int train_dispatcher_tid) {
  trainset->last_track_switch_time = 0;
  trainset->max_read_sensor_query_time = 0;

  trainset->train_dispatcher = train_dispatcher_tid;
  trainset->marklin_rx = marklin_rx_tid;

  for (unsigned int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * TRAINSET_NUM_SENSORS_PER_MODULE;
       ++i) {
    trainset->sensors_occupied[i] = false;
  }

  for (unsigned int i = 0; i < TRAINSET_NUM_MAX_SWITCHES; ++i) {
    trainset->switch_states[i] = DIRECTION_UNKNOWN;
  }

  for (unsigned int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    trainset->train_speeds[i] = 0;
  }

  circular_buffer_init(&trainset->reverse_buffer);

  uart_config_and_enable(UART_MARKLIN, BAUD_RATE, true, true);

  char cmd[] = {CMD_SENSOR_RESET_MODE};
  DispatchTrainCommand(trainset->train_dispatcher, CMD_SENSOR_RESET_MODE, 1);
}

static void send_command(struct Trainset *trainset, char arg1, char arg2) {
  char data[] = {arg1, arg2};
  DispatchTrainCommand(trainset->train_dispatcher, data, 2);
}

void trainset_set_train_speed(
    struct Trainset *trainset,
    int terminal_tid,
    uint8_t train,
    uint8_t speed) {
  int train_index = get_train_index(train);

  if (speed == SPEED_REVERSE_DIRECTION) {
    // reverse direction stops the train
    trainset->train_speeds[train_index] = 0;
  } else {
    trainset->train_speeds[train_index] = speed;
  }

  TerminalUpdateTrainSpeeds(terminal_tid, trainset->train_speeds);

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
  return trainset->train_speeds[get_train_index(train)];
}

void trainset_train_reverse(struct Trainset *trainset, int terminal_tid, uint8_t train) {
  int speed = trainset_get_speed(trainset, train);
  trainset_set_train_speed(&trainset, terminal_tid, train, 0);

  int reverse_task = Create(TRAIN_TASK_PRIORITY, train_reverse_task);
  struct TrainReverseNotifyRequest req = {.train = train, .speed = speed};
  Send(reverse_task, (const char *) &req, sizeof(req), NULL, 0);
}

void trainset_set_switch_direction(
    struct Trainset *trainset,
    int terminal_tid,
    int switch_number,
    int direction,
    uint64_t time) {
  send_command(trainset, direction, switch_number);
  trainset->last_track_switch_time = time;
  trainset->switch_states[switch_number] =
      direction == TRAINSET_DIRECTION_CURVED ? DIRECTION_CURVED : DIRECTION_STRAIGHT;
  TerminalUpdateSwitchStates(terminal_tid);
}

static void process_sensor_data(struct Trainset *trainset, char *raw_sensor_data) {
  // each feedback module has 2 numbers (contacts 1 to 8) and (contacts 9 to 16)
  for (int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * 2; ++i) {
    char ch = raw_sensor_data[i];

    // 1 byte (8 bits) and each char represents 8 sensors
    // the most significant bit represents the lowest sensor
    for (int j = 0; j < 8; ++j) {
      // take last bit each time
      // trainset->sensors_occupied[i * 8 + j] = (ch >> j) & 1;
      trainset->sensors_occupied[i * 8 + j] = (ch >> (7 - j)) & 1;
    }
  }
}

bool *trainset_get_sensor_data(struct Trainset *trainset) {
  return trainset->sensors_occupied;
}

enum SwitchDirection trainset_get_switch_state(struct Trainset *trainset, uint8_t switch_number) {
  return trainset->switch_states[switch_number];
}
