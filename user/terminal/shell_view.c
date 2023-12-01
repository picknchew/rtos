#include <stdarg.h>

#include "../train/track_reservations.h"
#include "terminal_screen.h"
#include "user/train/trainset.h"

#define DECISECOND_IN_CENTISECONDS 10
#define SECOND_IN_CENTISECONDS (DECISECOND_IN_CENTISECONDS * 10)
#define MINUTE_IN_CENTISECONDS (SECOND_IN_CENTISECONDS * 60)
#define HOUR_IN_CENTISECONDS (MINUTE_IN_CENTISECONDS * 60)

#define DISTANCE_LINE 22
#define VELOCITY_BASE_LINE 24
static int velocity_offset = 0;

#define LOG_BASE_LINE 1
static int log_offset = 0;

#define ZONE_RESERVATION_BASE_LINE 37
#define ZONE_RESERVATION_BASE_COL 1
// 20rows 50 cols
static const char *track_a =
    // 012345678901234567890123456789012345678901234567 8 9
    "********__******************__********************************************__**********        "
    "    \r\n"  // 0 0-49
    "      A1.2**          ** C13.14             B5.6            D3.4         E7.8         **      "
    "    \r\n"  // 1 50-99
    "******__**          **      __****************__**************__**********__******      __ "
    "D7.8   \r\n"  // 2 100
    "A13.14**          **    ** C11.12           **                  **        E5.6    **      **  "
    "    \r\n"  // 3 150
    "**__**          **    **               E15.16 __              __ E3.4            D5.6 __    "
    "**    \r\n"  // 4 200
    "A15.16        **  **                            **          **                          **    "
    "**  \r\n"  // 5 250
    "            ****                             E1.2 __      __ D1.2                           "
    "****  \r\n"  // 6 300
    "            __ A3.4                                 ******                                    "
    "**  \r\n"  // 7 350
    "            **                                        **                                      "
    "**  \r\n"  // 8 400
    "            **                                        **                                      "
    "**  \r\n"  // 9 450
    "            __ B15.16                               ******                                    "
    "**  \r\n"  // 10 500
    "            ****                             C1.2 __      __ B13.14                   E9.10 "
    "****  \r\n"  // 11 550
    " A11.12       **  **                            **          **                          __    "
    "**  \r\n"  // 12 600
    "**__**          **  **                   B3.4 __              __ D15.16               **    "
    "__    \r\n"  // 13 650
    " B7.8 **          **  **     C9.10          **   B1.2   D13.14  ** E13.14         **      ** "
    "D9.10\r\n"  // 14 700
    "**__**__** A9.10    **  ******__******************__******__**********__**********      **    "
    "    \r\n"  // 15 750
    " B11.12   **          ********__******************__******__**********__**************        "
    "    \r\n"  // 16 800
    "**__******__** A7.8          C5.6           **  C15.16   D11.12 ** E11.12                     "
    "    \r\n"  // 17 850
    " B9.10        ** A5.6                    C7.8 **              ** C3.4                         "
    "    \r\n"  // 18 900
    "**__**********__**************************__********************__****************************"
    "    \r\n";  // 19 950

static const char *track_b =
    // 012345678901234567890123456789012345678901234567 8 9
    "  "
    "****************************__********************__**************************__**********__**"
    "  \r\n"  // 19 950
    "                             C3.4 **              ** C7.8                    A5.6 **       "
    "B9.10  \r\n"  // 18 900
    "                        E11.12  **  D11.12   C15.16 **           C5.6          A7.8 "
    "**__******__**\r\n"  // 17 850
    "            **************__**********__******__******************__********          **   "
    "B11.12 \r\n"  // 16 800
    "        **      **********__**********__******__******************__******  **    A9.10 "
    "**__**__**\r\n"  // 15 750
    " D9.10**      **         E13.14 **  D13.14   B1.2   **           C9.10    **  **          ** "
    "B7.8 \r\n"  // 14 700
    "    __    **               D15.16 __              __ B3.4                   **  **   A11.12 "
    "__    \r\n"  // 13 650
    "  **    __ E9.10                    **          **                            **  **         "
    "**   \r\n"  // 12 600
    "  ****                         B13.14 __      __ C1.2                             ****       "
    "**   \r\n"  // 11 550
    "  **                                    ******                               B15.16 __       "
    "**   \r\n"  // 10 500
    "  **                                      **                                        **       "
    "**   \r\n"  // 9 450
    "  **                                      **                                        **       "
    "**   \r\n"  // 8 400
    "  **                                    ******                                 A3.4 __       "
    "**   \r\n"  // 7 350
    "  ****                           D1.2 __      __ E1.2                             ****       "
    "**   \r\n"  // 6 300
    "  **    **                          **          **                            **  **         "
    "**   \r\n"  // 5 250
    "    **    __ D5.6            E3.4 __              __ E15.16               **    **   A15.16 "
    "__    \r\n"  // 4 200
    "      **      **     E5,6       **                  **           C11.12 **    **          "
    "**A13.14\r\n"  // 3 150
    "   D7.8 __      ******__**********--**************--****************__      **          "
    "**__******\r\n"  // 2 100
    "          **         E7.8                                          C13.14 **          ** A1.2 "
    "    \r\n"  // 1 50-99
    "            "
    "**********__********************************************__*****************__********\r\n";  // 0 0-49

static const char *color[6] =
    {"\033[0;31m", "\033[0;32m", "\033[0;33m", "\033[0;34m", "\033[0;35m", "\033[0;36m"};

static const int SWITCHES_TITLE_ROW = 5;
static const int SWITCHES_ROW = 6;

static const int SECOND_COL = 40;

static const int THIRD_COL = 140;
static int TRAIN_INFO_ROW = 23;

static int log_num = 0;

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

static void init_train_zones(struct TerminalScreen *screen, char track) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, ZONE_RESERVATION_BASE_LINE, ZONE_RESERVATION_BASE_COL);
  if (track == 'A') {
    // zones_a_init();
    // terminal_puts(screen, track_a);
    terminal_printf(screen, track_a);

  } else {
    // zones_b_init();
    // terminal_puts(screen, track_b);
    terminal_printf(screen, track_b);
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
  terminal_puts(screen, "\033[32m");

  terminal_format_print(screen, fmt, va);
  // padding to clear previous values
  terminal_printf(screen, "%64c", ' ');

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
  // padding to clear previous values
  terminal_printf(screen, "%64c", ' ');
  terminal_restore_cursor(screen);
}

static void update_train_info(
    struct TerminalScreen *screen,
    int train,
    const char *pos_node,
    int pos_offset,
    const char *state,
    const char *next_sensor,
    int sensor_estimate,
    int sensor_eta_error,
    const char *dest,
    FixedPointInt speed,
    FixedPointInt accel
) {
  terminal_save_cursor(screen);

  terminal_move_cursor(screen, TRAIN_INFO_ROW + 1 + trainset_get_train_index(train), 0);

  terminal_printf(screen, "%9d", train);
  terminal_printf(screen, "%30s", state);
  terminal_printf(screen, "%12s", next_sensor);
  terminal_printf(screen, "%12d", sensor_estimate);
  terminal_printf(screen, "%14d", sensor_eta_error);
  terminal_printf(screen, "%4s + %6d", pos_node, pos_offset);
  terminal_printf(screen, "%14s", dest);
  terminal_printf(screen, "%15d", speed);
  terminal_printf(screen, "%d", accel);

  terminal_restore_cursor(screen);
}

static int SELECTED_TRACK_ROW = 5;

static void update_selected_track(struct TerminalScreen *screen, char track) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, SELECTED_TRACK_ROW, 60);
  terminal_print_title(screen, "Selected track: ");
  terminal_putc(screen, track);
  terminal_restore_cursor(screen);
  // init_train_zones(screen,track);
}

static void update_zone_reservation(struct TerminalScreen *screen, int zone, int train, int type) {
  terminal_save_cursor(screen);
  struct Zone zone_track = getZone(zone);

  for (int i = 0; i < zone_track.len; i++) {
    int row = zone_track.tracks[i] / 100;
    int col = zone_track.tracks[i] % 100;

    terminal_move_cursor(screen, row + ZONE_RESERVATION_BASE_LINE, col + ZONE_RESERVATION_BASE_COL);
    if (type == 0) {
      terminal_puts(screen, color[train % 6]);
    } else if (type == 1) {
      terminal_printf(screen, "\033[0;37m");
    }

    terminal_putc(screen, '*');
    terminal_putc(screen, '*');
  }

  terminal_printf(screen, "\033[0;37m");
  // terminal_move_cursor(screen, ZONE_RESERVATION_BASE_LINE + zone_offset,
  // ZONE_RESERVATION_BASE_COL);
  // ++zone_offset;
  // // extra space after speed to overwrite previous two digit numbers
  // if (type==0) {
  //   terminal_printf(screen, "ZONE %d is occupied by train %d ", zone, train);
  // } else if (type==1) {
  //   terminal_printf(screen, "ZONE %d is released by train %d ", zone, train);
  // }
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
  // padding to clear previous values
  terminal_printf(screen, "%20c", ' ');

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
      screen, "(%u%% of uptime) %u:%u:%u:%5u", idle_pct, hours, minutes, seconds, centiseconds
  );

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

static void log_print_va(struct TerminalScreen *screen, char *fmt, va_list va) {
  terminal_save_cursor(screen);
  terminal_move_cursor(screen, LOG_BASE_LINE + log_offset, THIRD_COL);
  ++log_offset;
  terminal_printf(screen, "[%d] ", log_num);
  terminal_format_print(screen, fmt, va);
  terminal_restore_cursor(screen);

  ++log_num;

  // restart at the top and overwrite
  terminal_save_cursor(screen);
  if (log_offset == 81) {
    for (int i = 0; i < 81; ++i) {
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
  log_num = 0;

  terminal_clear_screen(screen);
  terminal_hide_cursor(screen);
  init_switch_states(screen);
  init_train_speeds(screen);
  init_train_info(screen);
  // A is the default track
  update_selected_track(screen, 'A');
  init_train_zones(screen, 'A');
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
      update_zone_reservation,
      update_selected_track,
      init_train_zones,
  };

  return view;
}
