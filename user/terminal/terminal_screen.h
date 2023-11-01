#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "user/train/trainset.h"

struct TerminalScreen {
  int console_tx;
};

void terminal_screen_init(struct TerminalScreen *screen);
void terminal_update_train_speed(
    struct TerminalScreen *screen,
    int train_index,
    uint8_t train_speed);
void terminal_update_sensors(struct TerminalScreen *screen, bool *sensors, size_t sensors_len);
void terminal_update_status(struct TerminalScreen *screen, char *fmt, ...);
void terminal_update_status_va(struct TerminalScreen *screen, char *fmt, va_list va);
void terminal_update_switch_state(
    struct TerminalScreen *screen,
    int switch_num,
    enum SwitchDirection dir);
void terminal_update_idle(struct TerminalScreen *screen, uint64_t idle, int idle_pct);
void terminal_update_max_sensor_duration(
    struct TerminalScreen *screen,
    unsigned int max_sensor_query_duration);
void terminal_update_time(struct TerminalScreen *screen, uint64_t time);
void terminal_update_command(struct TerminalScreen *screen, char *command, unsigned int len);
