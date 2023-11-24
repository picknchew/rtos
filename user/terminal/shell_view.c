#include <stdarg.h>

#include "terminal_screen.h"
#include "user/train/trainset.h"

#define DECISECOND_IN_CENTISECONDS 10
#define SECOND_IN_CENTISECONDS (DECISECOND_IN_CENTISECONDS * 10)
#define MINUTE_IN_CENTISECONDS (SECOND_IN_CENTISECONDS * 60)
#define HOUR_IN_CENTISECONDS (MINUTE_IN_CENTISECONDS * 60)

#define DISTANCE_LINE 22
#define VELOCITY_BASE_LINE 24
static int velocity_offset = 0;

#define LOG_BASE_LINE 0
static int log_offset = 0;

static const int SWITCHES_TITLE_ROW = 5;
static const int SWITCHES_ROW = 6;

static const int SECOND_COL = 40;

static const int THIRD_COL = 140;
static int TRAIN_INFO_ROW = 23;

static void init_switch_states(struct TerminalScreen *screen) {
  velocity_offset = 0;
  log_offset = 0;
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

static void init_train_info(struct TerminalScreen *screen) {
  terminal_save_cursor(screen);

  terminal_move_cursor(screen, TRAIN_INFO_ROW, 0);
  terminal_print_title(screen, "Train");

  terminal_move_cursor(screen, TRAIN_INFO_ROW, 10);
  terminal_print_title(screen, "State");

  terminal_move_cursor(screen, TRAIN_INFO_ROW, 40);
  terminal_print_title(screen, "Next sensor");

  terminal_move_cursor(screen, TRAIN_INFO_ROW, 52);
  terminal_print_title(screen, "Sensor ETA");

  terminal_move_cursor(screen, TRAIN_INFO_ROW, 64);
  terminal_print_title(screen, "ETA error");

  terminal_move_cursor(screen, TRAIN_INFO_ROW, 78);
  terminal_print_title(screen, "Position");

  terminal_move_cursor(screen, TRAIN_INFO_ROW, 91);
  terminal_print_title(screen, "Destination");

  terminal_move_cursor(screen, TRAIN_INFO_ROW, 105);
  terminal_print_title(screen, "Speed");

  terminal_move_cursor(screen, TRAIN_INFO_ROW, 120);
  terminal_print_title(screen, "Acceleration");

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
  terminal_cursor_delete_line(screen);
  terminal_puts(screen, "\033[32m");

  terminal_format_print(screen, fmt, va);

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

static void update_train_info(
    struct TerminalScreen *screen,
    int train,
    char *pos_node,
    int pos_offset,
    char *state,
    char *next_sensor,
    int sensor_estimate,
    int sensor_eta_error,
    char *dest,
    FixedPointInt speed,
    FixedPointInt accel
) {
  terminal_save_cursor(screen);

  // delete previous train state
  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 135);
  terminal_puts(screen, "\033[1K");

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 0);
  terminal_printf(screen, "%d", train);

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 10);
  terminal_puts(screen, state);

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 40);
  terminal_puts(screen, next_sensor);

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 52);
  terminal_printf(screen, "%d", sensor_estimate);

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 64);
  terminal_printf(screen, "%d", sensor_eta_error);

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 78);
  terminal_printf(screen, "%s + %d", pos_node, pos_offset);

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 91);
  terminal_puts(screen, dest);

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 105);
  terminal_printf(screen, "%d", speed);

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 120);
  terminal_printf(screen, "%d", accel);

  terminal_restore_cursor(screen);
}

static int SELECTED_TRACK_ROW = 16;

static void update_selected_track(struct TerminalScreen *screen, char track) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, SELECTED_TRACK_ROW, SECOND_COL);
  terminal_print_title(screen, "Selected track: ");
  terminal_putc(screen, track);
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

static void
update_idle(struct TerminalScreen *screen, uint64_t idle, int idle_pct, int recent_idle_pct) {
  // guaranteed to be less than 64 bits
  unsigned int hours = idle / HOUR_IN_MICROSECONDS;
  unsigned int minutes = (idle % HOUR_IN_MICROSECONDS) / MINUTE_IN_MICROSECONDS;
  unsigned int seconds = (idle % MINUTE_IN_MICROSECONDS) / SECOND_IN_MICROSECONDS;
  unsigned int centiseconds = (idle % SECOND_IN_MICROSECONDS) / CENTISECOND_IN_MICROSECONDS;

  terminal_save_cursor(screen);
  terminal_move_cursor(screen, 0, SECOND_COL);

  terminal_print_title(screen, "Idle time: ");
  terminal_printf(
      screen, "(%u%% of uptime) %u:%u:%u:%u", idle_pct, hours, minutes, seconds, centiseconds
  );

  terminal_cursor_delete_line(screen);
  terminal_restore_cursor(screen);
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
  ++velocity_offset;
  terminal_printf(screen, "train %d at %d looptime %d velocity %d", train, speed, time, velocity);
  terminal_restore_cursor(screen);
}

int logOffset = 0;

static void log_print_va(struct TerminalScreen *screen, char *fmt, va_list va) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, LOG_BASE_LINE + log_offset, THIRD_COL);
  terminal_cursor_delete_line(screen);
  ++log_offset;
  terminal_format_print(screen, fmt, va);
  terminal_restore_cursor(screen);

  // restart at the top and overwrite
  terminal_save_cursor(screen);
  if (log_offset == 81) {
    for (int i = 0; i < 80; ++i) {
      terminal_move_cursor(screen, LOG_BASE_LINE + i, THIRD_COL);
      terminal_cursor_delete_line(screen);
    }

    log_offset = 0;
  }
  terminal_restore_cursor(screen);
}

static void print_next_sensor_time(struct TerminalScreen *screen, int time) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, 0, 70);
  terminal_print_title(screen, "Time to next sensor (ticks): ");
  terminal_printf(screen, "%d", time);
  terminal_restore_cursor(screen);
}

static void screen_init(struct TerminalScreen *screen) {
  terminal_clear_screen(screen);
  terminal_hide_cursor(screen);

  init_switch_states(screen);
  init_train_speeds(screen);
  init_train_info(screen);
  // A is the default track
  update_selected_track(screen, 'A');
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
      log_print_va,
      update_switch_state,
      update_idle,
      update_max_sensor_duration,
      update_time,
      update_command,
      print_loop_distance,
      print_loop_time,
      update_train_info,
      update_selected_track
  };

  return view;
}
