#ifndef _trainset_h_
#define _trainset_h_ 1

#include <stdbool.h>
#include <stdint.h>

#include "../circular_buffer.h"

#define TRAINSET_NUM_FEEDBACK_MODULES 5
#define TRAINSET_NUM_SENSORS_PER_MODULE 16

#define TRAINSET_NUM_MAX_SWITCHES 255
#define TRAINSET_NUM_TRAINS 7

extern const int TRAINSET_DIRECTION_STRAIGHT;
extern const int TRAINSET_DIRECTION_CURVED;
extern const int TRAINSET_TRAINS[];

struct Terminal;

enum SwitchDirection { DIRECTION_CURVED, DIRECTION_STRAIGHT, DIRECTION_UNKNOWN };

struct Trainset {
  enum SwitchDirection switch_states[TRAINSET_NUM_MAX_SWITCHES];
  bool sensors_occupied[TRAINSET_NUM_FEEDBACK_MODULES * TRAINSET_NUM_SENSORS_PER_MODULE];
  bool track_switch_delay;

  uint8_t train_speeds[7];

  uint64_t last_track_switch_time;
  uint64_t last_command_sent_time;
  uint64_t last_read_sensor_query_time;

  uint64_t max_read_sensor_query_time;

  struct CircularBuffer read_buffer;
  struct CircularBuffer write_buffer;
  // trains that need to be reversed.
  struct CircularBuffer reverse_buffer;
};

void trainset_init(struct Trainset *trainset,int marklin_rx_tid, int marklin_tx_tid);
void trainset_set_train_speed(struct Trainset *trainset, struct Terminal *terminal, uint8_t train, uint8_t speed);
void trainset_train_reverse(struct Trainset *trainset, struct Terminal *terminal, uint8_t train, uint64_t time);
void trainset_set_switch_direction(struct Trainset *trainset, struct Terminal *terminal, int switch_number, int direction, uint64_t time);
bool *trainset_get_sensor_data(struct Trainset *trainset);
void trainset_tick(struct Trainset *trainset, uint64_t time, struct Terminal *terminal);
uint8_t *trainset_get_train_speeds(struct Trainset *trainset);
bool trainset_is_valid_train(uint8_t train);
enum SwitchDirection trainset_get_switch_state(struct Trainset *trainset, uint8_t switch_number);

#endif /* trainset.h */
