#pragma once

#include "../circular_buffer.h"
#include "trainset.h"

#define BUFFER_SIZE 64

struct Terminal {
  char command_buffer[BUFFER_SIZE];
  size_t command_len;
  uint64_t screen_last_updated;
  struct CircularBuffer write_buffer;

  int console_rx;
  int console_tx;
};

/**
 * Returns 1 if we should quit the program, otherwise returns 0;
 */
int terminal_tick(struct Terminal *terminal, uint64_t time);
void terminal_init(
    struct Terminal *terminal,
    int train_tid,
    int console_rx_tid,
    int console_tx_tid);
void terminal_update_train_speeds(struct Terminal *terminal, uint8_t *train_speeds);
void terminal_update_sensors(struct Terminal *terminal, bool *sensors, size_t sensors_len);
void terminal_update_status(struct Terminal *terminal, char *fmt, ...);
void terminal_update_switch_states(struct Terminal *terminal, int train_tid);
void terminal_update_max_sensor_duration(
    struct Terminal *terminal,
    unsigned int max_sensor_query_duration);
void terminal_update_time(struct Terminal *terminal, uint64_t time);
void terminal_update_command(struct Terminal *terminal);
int terminal_execute_command(struct Terminal *terminal, int train_tid, char *command);
