#include <stdarg.h>

#include "terminal_screen.h"
#include "user/train/trainset.h"

#define DECISECOND_IN_CENTISECONDS 10
#define SECOND_IN_CENTISECONDS (DECISECOND_IN_CENTISECONDS * 10)
#define MINUTE_IN_CENTISECONDS (SECOND_IN_CENTISECONDS * 60)
#define HOUR_IN_CENTISECONDS (MINUTE_IN_CENTISECONDS * 60)

#define DISTANCE_LINE 22
#define VELOCITY_BASE_LINE 24
int velocity_offset = 0;

static const int SWITCHES_TITLE_ROW = 5;
static const int SWITCHES_ROW = 6;

static void init_switch_states(struct TerminalScreen *screen) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, SWITCHES_TITLE_ROW, 1);
  terminal_print_title(screen, "Switch States:");

  int offset = 0;
  for (int switch_num = 1; switch_num <= 9; ++switch_num) {
    terminal_move_cursor(screen, SWITCHES_ROW + offset, 0);
    ++offset;

    terminal_printf(screen, "%d: ?", switch_num);
  }

  offset = 0;
  for (int switch_num = 10; switch_num <= 18; ++switch_num) {
    terminal_move_cursor(screen, SWITCHES_ROW + offset, 10);
    ++offset;

    terminal_printf(screen, "%d: ?", switch_num);
  }

  offset = 0;
  for (int switch_num = 153; switch_num <= 156; ++switch_num) {
    terminal_move_cursor(screen, SWITCHES_ROW + offset, 20);
    ++offset;

    terminal_printf(screen, "%d: ?", switch_num);
  }

  terminal_restore_cursor(screen);
}

static const int TRAIN_SPEEDS_TITLE_ROW = 5;
static const int TRAIN_SPEEDS_COL = 40;
static const int TRAIN_SPEEDS_ROW = 6;

static void init_train_speeds(struct TerminalScreen *screen) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, TRAIN_SPEEDS_TITLE_ROW, TRAIN_SPEEDS_COL);
  terminal_print_title(screen, "Train Speeds:");

  int offset = 0;
  for (unsigned int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    terminal_move_cursor(screen, TRAIN_SPEEDS_ROW + offset, TRAIN_SPEEDS_COL);
    ++offset;

    terminal_printf(screen, "%d: 0 ", TRAINSET_TRAINS[i]);
  }

  terminal_restore_cursor(screen);
}

static char get_switch_dir_char(enum SwitchDirection dir) {
  switch (dir) {
    case DIRECTION_UNKNOWN:
      return '?';
    case DIRECTION_STRAIGHT:
      return 'S';
    default:
      return 'C';
  }
}

static void update_status_va(struct TerminalScreen *screen, char *fmt, va_list va) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, 20, 1);
  terminal_puts(screen, "\033[32m");

  terminal_format_print(screen, fmt, va);

  terminal_cursor_delete_line(screen);
  terminal_cursor_reset_text(screen);
  terminal_restore_cursor(screen);
}

static void
update_switch_state(struct TerminalScreen *screen, int switch_num, enum SwitchDirection dir) {
  terminal_save_cursor(screen);

  int switch_index = switch_num;
  if (switch_index >= 153) {
    switch_index -= 134;
  }

  switch_index -= 1;

  int row_offset = switch_index % 9;
  int col_offset = switch_index / 9;

  terminal_move_cursor(screen, SWITCHES_ROW + row_offset, col_offset * 10);
  terminal_printf(screen, "%d: %c", switch_num, get_switch_dir_char(dir));
  terminal_restore_cursor(screen);
}

static void update_time(struct TerminalScreen *screen, uint64_t time) {
  // guaranteed to be less than 64 bits
  int hours = time / HOUR_IN_CENTISECONDS;
  int minutes = (time % HOUR_IN_CENTISECONDS) / MINUTE_IN_CENTISECONDS;
  int seconds = (time % MINUTE_IN_CENTISECONDS) / SECOND_IN_CENTISECONDS;
  int deciseconds = (time % SECOND_IN_CENTISECONDS) / DECISECOND_IN_CENTISECONDS;

  terminal_save_cursor(screen);
  terminal_cursor_move_top_left(screen);
  terminal_printf(
      screen, "\033[37mCurrent Uptime: %d:%d:%d:%d    ", hours, minutes, seconds, deciseconds
  );
  terminal_restore_cursor(screen);
}

static void
update_train_speed(struct TerminalScreen *screen, int train_index, uint8_t train_speed) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, TRAIN_SPEEDS_ROW + train_index, TRAIN_SPEEDS_COL);

  // extra space after speed to overwrite previous two digit numbers
  terminal_printf(screen, "%d: %d ", TRAINSET_TRAINS[train_index], train_speed);
  terminal_restore_cursor(screen);
}

static void update_command(struct TerminalScreen *screen, char *command, unsigned int len) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, 19, 1);
  terminal_puts(screen, "> ");
  terminal_putl(screen, command, len);
  terminal_cursor_delete_line(screen);
  terminal_restore_cursor(screen);
}

static void
update_max_sensor_duration(struct TerminalScreen *screen, unsigned int max_sensor_query_duration) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, 3, 1);
  terminal_printf(screen, "Sensor query duration in ticks (max): %u", max_sensor_query_duration);
  terminal_restore_cursor(screen);
}

static void update_sensors(struct TerminalScreen *screen, bool *sensors, size_t sensors_len) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, 16, 1);

  terminal_print_title(screen, "Recently triggered sensors:");
  for (unsigned int i = 0; i < sensors_len; ++i) {
    if (sensors[i]) {
      char ch = 'A' + i / 16;
      terminal_printf(screen, " %c%d", ch, i % 16 + 1);
    }
  }
  terminal_cursor_delete_line(screen);

  terminal_restore_cursor(screen);
}

#define CENTISECOND_IN_MICROSECONDS 10000u
#define SECOND_IN_MICROSECONDS (CENTISECOND_IN_MICROSECONDS * 100)
#define MINUTE_IN_MICROSECONDS (SECOND_IN_MICROSECONDS * 60)
#define HOUR_IN_MICROSECONDS (MINUTE_IN_MICROSECONDS * 60)

static void update_idle(struct TerminalScreen *screen, uint64_t idle, int idle_pct) {
  // guaranteed to be less than 64 bits
  unsigned int hours = idle / HOUR_IN_MICROSECONDS;
  unsigned int minutes = (idle % HOUR_IN_MICROSECONDS) / MINUTE_IN_MICROSECONDS;
  unsigned int seconds = (idle % MINUTE_IN_MICROSECONDS) / SECOND_IN_MICROSECONDS;
  unsigned int centiseconds = (idle % SECOND_IN_MICROSECONDS) / CENTISECOND_IN_MICROSECONDS;

  terminal_update_status(
      screen,
      "idle_task: idle time (%u%% of uptime) %u:%u:%u:%u\r\n",
      idle_pct,
      hours,
      minutes,
      seconds,
      centiseconds
  );
}

static void print_loop_distance(
    struct TerminalScreen *screen,
    const char *begin,
    const char *end,
    int distance
) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, DISTANCE_LINE, 1);
  terminal_printf(screen, "START FROM %s TO %s distance: %d ", begin, end, distance);
  terminal_restore_cursor(screen);
}

static void
print_loop_time(struct TerminalScreen *screen, int train, int speed, int time, int velocity) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, VELOCITY_BASE_LINE + velocity_offset, 1);
  velocity_offset++;
  terminal_printf(screen, "train %d at %d looptime %d velocity %d", train, speed, time, velocity);
  terminal_restore_cursor(screen);
}

static void screen_init(struct TerminalScreen *screen) {
  terminal_clear_screen(screen);
  terminal_hide_cursor(screen);

  init_switch_states(screen);
  init_train_speeds(screen);
  update_sensors(screen, NULL, 0);
  update_command(screen, "", 0);
  update_max_sensor_duration(screen, 0);
}

struct TerminalView shell_view_create() {
  struct TerminalView view = {
      screen_init,
      update_train_speed,
      update_sensors,
      update_status_va,
      update_switch_state,
      update_idle,
      update_max_sensor_duration,
      update_time,
      update_command,
      print_loop_distance,
      print_loop_time
  };

  return view;
}