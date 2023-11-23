#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "circular_buffer.h"
#include "trackdata/track_data.h"

#define TRAINSET_NUM_MAX_SWITCHES 255
#define TRAINSET_NUM_TRAINS 8

extern const int TRAINSET_TRAINS[];

enum SwitchDirection { DIRECTION_CURVED, DIRECTION_STRAIGHT, DIRECTION_UNKNOWN };

enum TrainSetDirection { TRAINSET_DIRECTION_STRAIGHT = 0x21, TRAINSET_DIRECTION_CURVED = 0x22 };

extern const unsigned int DELAY_OFF_LAST_SOLENOID;
extern const unsigned int DELAY_REVERSE;

extern const int SPEED_REVERSE_DIRECTION;

struct Trainset {
  enum SwitchDirection switch_states[TRAINSET_NUM_MAX_SWITCHES];
  bool *sensors_occupied;

  uint8_t train_speeds[TRAINSET_NUM_TRAINS];

  uint64_t last_track_switch_time;

  // task tids
  int train_dispatcher;
};

void trainset_init(struct Trainset *trainset, int train_dispatcher_tid);
int trainset_get_train_index(uint8_t train);
int trainset_get_sensor_index(char *sensor);
void trainset_set_train_speed(
    struct Trainset *trainset,
    int terminal_tid,
    uint8_t train,
    uint8_t speed
);
uint8_t trainset_get_speed(struct Trainset *trainset, uint8_t train);
void trainset_train_reverse(struct Trainset *trainset, int terminal_tid, uint8_t train);
void trainset_set_switch_direction(
    struct Trainset *trainset,
    int terminal_tid,
    int switch_number,
    int direction,
    uint64_t time
);
void trainset_update_sensor_data(struct Trainset *trainset, bool *sensor_data);
bool *trainset_get_sensor_data(struct Trainset *trainset);
bool trainset_is_valid_train(uint8_t train);
void trainset_set_track(struct Trainset *trainset, char track_type);
enum SwitchDirection trainset_get_switch_state(struct Trainset *trainset, uint8_t switch_number);
void train_task();
