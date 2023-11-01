#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "circular_buffer.h"

#define TRAINSET_NUM_FEEDBACK_MODULES 5
#define TRAINSET_NUM_SENSORS_PER_MODULE 16

#define TRAINSET_NUM_MAX_SWITCHES 255
#define TRAINSET_NUM_TRAINS 8

extern const int TRAINSET_DIRECTION_STRAIGHT;
extern const int TRAINSET_DIRECTION_CURVED;
extern const int TRAINSET_TRAINS[];

enum SwitchDirection { DIRECTION_CURVED, DIRECTION_STRAIGHT, DIRECTION_UNKNOWN };

extern const unsigned int DELAY_OFF_LAST_SOLENOID;
extern const unsigned int DELAY_REVERSE;

extern const int SPEED_REVERSE_DIRECTION;

struct Trainset {
  enum SwitchDirection switch_states[TRAINSET_NUM_MAX_SWITCHES];
  bool sensors_occupied[TRAINSET_NUM_FEEDBACK_MODULES * TRAINSET_NUM_SENSORS_PER_MODULE];

  uint8_t train_speeds[TRAINSET_NUM_TRAINS];

  uint64_t last_track_switch_time;
  uint64_t max_read_sensor_query_time;

  // task tids
  int train_dispatcher;
};

void trainset_init(struct Trainset *trainset, int train_dispatcher_tid);
void trainset_set_train_speed(
    struct Trainset *trainset,
    int terminal_tid,
    uint8_t train,
    uint8_t speed);
uint8_t trainset_get_speed(struct Trainset *trainset, uint8_t train);
void trainset_train_reverse(struct Trainset *trainset, int terminal_tid, uint8_t train);
void trainset_set_switch_direction(
    struct Trainset *trainset,
    int terminal_tid,
    int switch_number,
    int direction,
    uint64_t time);
bool *trainset_get_sensor_data(struct Trainset *trainset);
bool trainset_is_valid_train(uint8_t train);
enum SwitchDirection trainset_get_switch_state(struct Trainset *trainset, uint8_t switch_number);
void trainset_process_sensor_data(struct Trainset *trainset, char *raw_sensor_data);
void train_task();
