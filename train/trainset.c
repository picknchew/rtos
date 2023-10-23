#include "trainset.h"

#include <stdbool.h>

static const int LINE_TRAINSET = 2;
static const int BAUD_RATE = 2400;

// delay to switch off last solenoid in microseconds (150ms)
static const unsigned int DELAY_OFF_LAST_SOLENOID = 150e3;
// delay between commands to Marklin in microseconds (100ms)
static const unsigned int DELAY_COMMAND_SEND = 100e3;
// delay between braking and reversing (2s)
static const unsigned int DELAY_REVERSE = 2e6;

static const int CMD_OFF_LAST_SOLENOID = 0x20;
static const int CMD_READ_ALL_SENSORS = 0x80 + TRAINSET_NUM_FEEDBACK_MODULES;
static const int CMD_SENSOR_RESET_MODE = 0xC0;

// offset to add to speed to turn on function on train (headlights)
static const int SPEED_OFFSET_FUNCTION = 16;
static const int SPEED_REVERSE_DIRECTION = 15;

const int TRAINSET_DIRECTION_STRAIGHT = 0x21;
const int TRAINSET_DIRECTION_CURVED = 0x22;

const int TRAINSET_TRAINS[] = {1, 2, 24, 47, 54, 58, 78};

// forward declarations
void terminal_update_train_speeds(struct Terminal *terminal, uint8_t *train_speeds);
void terminal_update_sensors(struct Terminal *terminal, bool *sensors, size_t sensors_len);
void terminal_update_status(struct Terminal *terminal, char *fmt, ...);
void terminal_update_switch_states(struct Terminal *terminal);
void terminal_update_max_sensor_duration(struct Terminal *terminal, unsigned int max_sensor_query_duration);

static void putc(struct Trainset *trainset, char ch) {
  circular_buffer_write(&trainset->write_buffer, ch);
}

static void read_sensor_data(struct Trainset *trainset) {
  putc(trainset, CMD_READ_ALL_SENSORS);
}

static int get_train_index(uint8_t train) {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    if (train == TRAINSET_TRAINS[i]) {
      return i;
    }
  }

  return 0;
}

void trainset_init(struct Trainset *trainset) {
  trainset->last_track_switch_time = 0;
  trainset->last_command_sent_time = 0;
  trainset->last_read_sensor_query_time = 0;
  trainset->max_read_sensor_query_time = 0;
  trainset->track_switch_delay = false;

  for (unsigned int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * TRAINSET_NUM_SENSORS_PER_MODULE; ++i) {
    trainset->sensors_occupied[i] = false;
  }

  for (unsigned int i = 0; i < TRAINSET_NUM_MAX_SWITCHES; ++i) {
    trainset->switch_states[i] = DIRECTION_UNKNOWN;
  }

  for (unsigned int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    trainset->train_speeds[i] = 0;
  }

  circular_buffer_init(&trainset->write_buffer);
  circular_buffer_init(&trainset->read_buffer);
  circular_buffer_init(&trainset->reverse_buffer);

  uart_config_and_enable(LINE_TRAINSET, BAUD_RATE, true);

  putc(trainset, CMD_SENSOR_RESET_MODE);
  // get initial sensor data
  read_sensor_data(trainset);
}

static void puts(struct Trainset *trainset, char *data) {
  while (*data) {
    putc(trainset, *data);
    ++data;
  }
}

static void send_command(struct Trainset *trainset, char arg1, char arg2) {
  char data[] = {arg1, arg2};
  puts(trainset, data);
}

void trainset_set_train_speed(struct Trainset *trainset, struct Terminal *terminal, uint8_t train, uint8_t speed) {
  int train_index = get_train_index(train);

  if (speed == SPEED_REVERSE_DIRECTION) {
    // reverse direction stops the train
    trainset->train_speeds[train_index] = 0;
  } else {
    trainset->train_speeds[train_index] = speed;
  }

  terminal_update_train_speeds(terminal, trainset->train_speeds);

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

static uint8_t get_speed(struct Trainset *trainset, uint8_t train) {
  return trainset->train_speeds[get_train_index(train)];
}

void trainset_train_reverse(struct Trainset *trainset, struct Terminal *terminal, uint8_t train, uint64_t time) {
  int speed = get_speed(trainset, train);

  // this stops the train and reverses the direction for the next set train speed commmand
  trainset_set_train_speed(trainset, terminal, train, 0);

  circular_buffer_write_int64(&trainset->reverse_buffer, time);
  circular_buffer_write_int8(&trainset->reverse_buffer, train);
  circular_buffer_write_int8(&trainset->reverse_buffer, speed);
}

void trainset_set_switch_direction(struct Trainset *trainset, struct Terminal *terminal, int switch_number, int direction, uint64_t time) {
  send_command(trainset, direction, switch_number);
  trainset->track_switch_delay = true;
  trainset->last_track_switch_time = time;
  trainset->switch_states[switch_number] = direction == TRAINSET_DIRECTION_CURVED ? DIRECTION_CURVED : DIRECTION_STRAIGHT;
  terminal_update_switch_states(terminal);
}

static bool sensor_data_received(struct Trainset *trainset) {
  // check size of circular buffer to see if it matches expected
  return circular_buffer_size(&trainset->read_buffer) == TRAINSET_NUM_FEEDBACK_MODULES * 2;
}

static void process_sensor_data(struct Trainset *trainset) {
  // each feedback module has 2 numbers (contacts 1 to 8) and (contacts 9 to 16)
  for (int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * 2; ++i) {
    char ch = circular_buffer_read(&trainset->read_buffer);

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

uint8_t *trainset_get_train_speeds(struct Trainset *trainset) {
  return trainset->train_speeds;
}

enum SwitchDirection trainset_get_switch_state(struct Trainset *trainset, uint8_t switch_number) {
  return trainset->switch_states[switch_number];
}

// SW 1 - 18
// SW 153 - 156
// SW99
// SW9a
// SW9b
// SW9c
void trainset_tick(struct Trainset *trainset, uint64_t time, struct Terminal *terminal) {
  if (time - trainset->last_command_sent_time >= DELAY_COMMAND_SEND && !circular_buffer_is_empty(&trainset->write_buffer)) {
    char ch = circular_buffer_read(&trainset->write_buffer);
    uart_putc(LINE_TRAINSET, ch);
    trainset->last_command_sent_time = time;

    // timings
    if (ch == CMD_READ_ALL_SENSORS) {
      trainset->last_read_sensor_query_time = time;
    }
  }

  if (!circular_buffer_is_empty(&trainset->reverse_buffer) && 
      time - circular_buffer_peek_int64(&trainset->reverse_buffer) >= DELAY_REVERSE) {
    // intentionally ignore return value (only remove)
    circular_buffer_read_int64(&trainset->reverse_buffer);

    uint8_t train = circular_buffer_read_int8(&trainset->reverse_buffer);
    uint8_t speed = circular_buffer_read_int8(&trainset->reverse_buffer);

    terminal_update_status(terminal, "Reversing train..");
    trainset_set_train_speed(trainset, terminal, train, SPEED_REVERSE_DIRECTION);
    trainset_set_train_speed(trainset, terminal, train, speed);
  }

  if (trainset->track_switch_delay && time - trainset->last_track_switch_time >= DELAY_OFF_LAST_SOLENOID) {
    putc(trainset, CMD_OFF_LAST_SOLENOID);
    trainset->track_switch_delay = false;
  }

  if (sensor_data_received(trainset)) {
    if (trainset->last_read_sensor_query_time) {
      uint64_t duration = time - trainset->last_read_sensor_query_time;

      if (duration > trainset->max_read_sensor_query_time) {
        trainset->max_read_sensor_query_time = duration;
        terminal_update_max_sensor_duration(terminal, trainset->max_read_sensor_query_time);
      }
    }

    process_sensor_data(trainset);
    read_sensor_data(trainset);
    terminal_update_sensors(terminal, trainset_get_sensor_data(trainset), TRAINSET_NUM_FEEDBACK_MODULES * TRAINSET_NUM_SENSORS_PER_MODULE);
  }

  if (uart_hasc(LINE_TRAINSET)) {
    char in = uart_getc(LINE_TRAINSET);
    circular_buffer_write(&trainset->read_buffer, in);
  }
}
