#pragma once

#include <stdarg.h>
#include <stdint.h>

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

void terminal_init(struct Terminal *terminal, int console_rx_tid, int console_tx_tid);
void terminal_update_train_speed(struct Terminal *terminal, int train_index, uint8_t train_speed);
void terminal_update_sensors(struct Terminal *terminal, bool *sensors, size_t sensors_len);
void terminal_update_status(struct Terminal *terminal, char *fmt, ...);
void terminal_update_status_va(struct Terminal *terminal, char *fmt, va_list va);
void terminal_update_switch_state(
    struct Terminal *terminal,
    int switch_num,
    enum SwitchDirection dir);
void terminal_update_idle(struct Terminal *terminal, uint64_t idle, int idle_pct);
void terminal_update_max_sensor_duration(
    struct Terminal *terminal,
    unsigned int max_sensor_query_duration);
void terminal_update_time(struct Terminal *terminal, uint64_t time);
void terminal_update_command(struct Terminal *terminal);
bool terminal_handle_keypress(struct Terminal *terminal, int train_tid, char c);
bool terminal_execute_command(struct Terminal *terminal, int train_tid, char *command);
void terminal_clear_command_buffer(struct Terminal *terminal);
