#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "user/train/trainset.h"

struct TerminalScreen;

struct TerminalView {
  void (*screen_init)(struct TerminalScreen *);
  void (*update_train_speed)(struct TerminalScreen *, int, uint8_t);
  void (*update_sensors)(struct TerminalScreen *, bool *, size_t);
  void (*update_status_va)(struct TerminalScreen *, char *, va_list);
  void (*log_print_va)(struct TerminalScreen *, char *, va_list);
  void (*update_switch_state)(struct TerminalScreen *, int, enum SwitchDirection);
  void (*update_idle)(struct TerminalScreen *, uint64_t, int);
  void (*update_max_sensor_duration)(struct TerminalScreen *, unsigned int);
  void (*update_time)(struct TerminalScreen *, uint64_t);
  void (*update_command)(struct TerminalScreen *, char *, unsigned int);
  void (*print_loop_distance)(struct TerminalScreen *, const char *, const char *, int);
  void (*print_loop_time)(struct TerminalScreen *, int, int, int, int);
  // void (*print_next_sensor)(struct TerminalScreen *, int);
};

struct TerminalScreen {
  int console_tx;
  struct TerminalView view;
};

void terminal_format_print(struct TerminalScreen *screen, char *fmt, va_list va);
void terminal_printf(struct TerminalScreen *screen, char *fmt, ...);
void terminal_move_cursor(struct TerminalScreen *screen, int r, int c);
void terminal_save_cursor(struct TerminalScreen *screen);
void terminal_restore_cursor(struct TerminalScreen *screen);
void terminal_cursor_reset_text(struct TerminalScreen *screen);
void terminal_print_title(struct TerminalScreen *screen, char *title);
void terminal_clear_screen(struct TerminalScreen *screen);
void terminal_cursor_move_top_left(struct TerminalScreen *screen);
void terminal_cursor_delete_line(struct TerminalScreen *screen);
void terminal_hide_cursor(struct TerminalScreen *screen);
void terminal_show_cursor(struct TerminalScreen *screen);
void terminal_update_status(struct TerminalScreen *screen, char *fmt, ...);
void terminal_putc(struct TerminalScreen *screen, char ch);
void terminal_putl(struct TerminalScreen *screen, const char *buf, size_t blen);
void terminal_puts(struct TerminalScreen *screen, const char *str);

inline void terminal_screen_init(struct TerminalScreen *screen, int console_tx, struct TerminalView view) {
  screen->console_tx = console_tx;
  screen->view = view;
  screen->view.screen_init(screen);
}

inline void
terminal_update_train_speed(struct TerminalScreen *screen, int train_index, uint8_t train_speed) {
  screen->view.update_train_speed(screen, train_index, train_speed);
}

inline void
terminal_update_sensors(struct TerminalScreen *screen, bool *sensors, size_t sensors_len) {
  screen->view.update_sensors(screen, sensors, sensors_len);
}

inline void terminal_update_status_va(struct TerminalScreen *screen, char *fmt, va_list va) {
  screen->view.update_status_va(screen, fmt, va);
}

inline void terminal_log_print_va(struct TerminalScreen *screen, char *fmt, va_list va) {
  screen->view.log_print_va(screen, fmt, va);
}

inline void terminal_update_switch_state(
    struct TerminalScreen *screen,
    int switch_num,
    enum SwitchDirection dir
) {
  screen->view.update_switch_state(screen, switch_num, dir);
}

inline void terminal_update_idle(struct TerminalScreen *screen, uint64_t idle, int idle_pct) {
  screen->view.update_idle(screen, idle, idle_pct);
}

inline void terminal_update_max_sensor_duration(
    struct TerminalScreen *screen,
    unsigned int max_sensor_query_duration
) {
  screen->view.update_max_sensor_duration(screen, max_sensor_query_duration);
}

inline void terminal_update_time(struct TerminalScreen *screen, uint64_t time) {
  screen->view.update_time(screen, time);
}

inline void
terminal_update_command(struct TerminalScreen *screen, char *command, unsigned int len) {
  screen->view.update_command(screen, command, len);
}

inline void terminal_print_loop_distance(
    struct TerminalScreen *screen,
    const char *begin,
    const char *end,
    int distance
) {
  screen->view.print_loop_distance(screen, begin, end, distance);
}

inline void terminal_print_loop_time(
    struct TerminalScreen *screen,
    int train,
    int speed,
    int time,
    int velocity
) {
  screen->view.print_loop_time(screen, train, speed, time, velocity);
}
