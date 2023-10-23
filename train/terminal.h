#ifndef _terminal_h_
#define _terminal_h_ 1

#include "../circular_buffer.h"
#include "trainset.h"

#define BUFFER_SIZE 64

struct Terminal {
  char command_buffer[BUFFER_SIZE];
  size_t command_len;
  uint64_t screen_last_updated;
  uint64_t time_last_updated;
  struct CircularBuffer write_buffer;
  struct Trainset *trainset;
};

/**
 * Returns 1 if we should quit the program, otherwise returns 0;
 */
int terminal_tick(struct Terminal *terminal, uint64_t time, unsigned int max_loop_duration);
void terminal_init(struct Terminal *terminal, struct Trainset *trainset);
void terminal_update_train_speeds(struct Terminal *terminal, uint8_t *train_speeds);
void terminal_update_sensors(struct Terminal *terminal, bool *sensors, size_t sensors_len);
void terminal_update_status(struct Terminal *terminal, char *fmt, ...);
void terminal_update_switch_states(struct Terminal *terminal);
void terminal_update_max_sensor_duration(struct Terminal *terminal, unsigned int max_sensor_query_duration);

#endif /* terminal.h */