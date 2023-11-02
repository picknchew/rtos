#include "terminal_screen.h"

#include "user/server/io_server.h"
#include "user/train/trainset.h"
#include "util.h"

static const char SEQ_CLEAR_SCREEN[] = "\033[2J";
static const char SEQ_CURSOR_DELETE_LINE[] = "\033[K";

static const char SEQ_CURSOR_HIDE[] = "\033[?25l";
static const char SEQ_CURSOR_SHOW[] = "\033[?25h";

static const char SEQ_CURSOR_MOVE_TOP_LEFT[] = "\033[H";

#define DECISECOND_IN_CENTISECONDS 10
#define SECOND_IN_CENTISECONDS (DECISECOND_IN_CENTISECONDS * 10)
#define MINUTE_IN_CENTISECONDS (SECOND_IN_CENTISECONDS * 60)
#define HOUR_IN_CENTISECONDS (MINUTE_IN_CENTISECONDS * 60)

#define DISTANCE_LINE 22
#define VELOCITY_BASE_LINE 23
int velocity_offset = 0;

const char TEXT_RESET[] = "\033[0m";

static void putc(struct TerminalScreen *screen, char ch) {
  Putc(screen->console_tx, ch);
}

static void putl(struct TerminalScreen *screen, const char *buf, size_t blen) {
  Putl(screen->console_tx, (const unsigned char *) buf, blen);
}

static void puts(struct TerminalScreen *screen, const char *str) {
  putl(screen, str, strlen(str));
}

static void format_print(struct TerminalScreen *screen, char *fmt, va_list va) {
  char bf[12];
  char ch;

  while ((ch = *(fmt++))) {
    if (ch != '%') {
      putc(screen, ch);
    } else {
      ch = *(fmt++);
      switch (ch) {
        case 'u':
          ui2a(va_arg(va, unsigned int), 10, bf);
          puts(screen, bf);
          break;
        case 'd':
          i2a(va_arg(va, int), bf);
          puts(screen, bf);
          break;
        case 'x':
          ui2a(va_arg(va, unsigned int), 16, bf);
          puts(screen, bf);
          break;
        case 'c':
          putc(screen, va_arg(va, int));
          break;
        case 's':
          puts(screen, va_arg(va, char *));
          break;
        case '%':
          putc(screen, ch);
          break;
        case '\0':
          return;
      }
    }
  }
}

static void printf(struct TerminalScreen *screen, char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  format_print(screen, fmt, va);
  va_end(va);
}

static void move_cursor(struct TerminalScreen *screen, int r, int c) {
  printf(screen, "\033[%d;%dH", r, c);
}

static void save_cursor(struct TerminalScreen *screen) {
  puts(screen, "\0337");
}

static void restore_cursor(struct TerminalScreen *screen) {
  puts(screen, "\0338");
}

static void print_title(struct TerminalScreen *screen, char *title) {
  printf(screen, "\033[36m%s", title);
  puts(screen, TEXT_RESET);
}

void terminal_update_status_va(struct TerminalScreen *screen, char *fmt, va_list va) {
  save_cursor(screen);
  move_cursor(screen, 20, 1);
  puts(screen, "\033[32m");

  format_print(screen, fmt, va);

  puts(screen, SEQ_CURSOR_DELETE_LINE);
  puts(screen, TEXT_RESET);
  restore_cursor(screen);
}

void terminal_update_status(struct TerminalScreen *screen, char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  terminal_update_status_va(screen, fmt, va);
  va_end(va);
}

static const int SWITCHES_TITLE_ROW = 5;
static const int SWITCHES_ROW = 6;

void terminal_init_switch_states(struct TerminalScreen *screen) {
  save_cursor(screen);
  move_cursor(screen, SWITCHES_TITLE_ROW, 1);
  print_title(screen, "Switch States:");

  int offset = 0;
  for (int switch_num = 1; switch_num <= 9; ++switch_num) {
    move_cursor(screen, SWITCHES_ROW + offset, 0);
    ++offset;

    printf(screen, "%d: ?", switch_num);
  }

  offset = 0;
  for (int switch_num = 10; switch_num <= 18; ++switch_num) {
    move_cursor(screen, SWITCHES_ROW + offset, 10);
    ++offset;

    printf(screen, "%d: ?", switch_num);
  }

  offset = 0;
  for (int switch_num = 153; switch_num <= 156; ++switch_num) {
    move_cursor(screen, SWITCHES_ROW + offset, 20);
    ++offset;

    printf(screen, "%d: ?", switch_num);
  }

  restore_cursor(screen);
}

static const int TRAIN_SPEEDS_TITLE_ROW = 5;
static const int TRAIN_SPEEDS_COL = 40;
static const int TRAIN_SPEEDS_ROW = 6;

static void terminal_init_train_speeds(struct TerminalScreen *screen) {
  save_cursor(screen);
  move_cursor(screen, TRAIN_SPEEDS_TITLE_ROW, TRAIN_SPEEDS_COL);
  print_title(screen, "Train Speeds:");

  int offset = 0;
  for (unsigned int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    move_cursor(screen, TRAIN_SPEEDS_ROW + offset, TRAIN_SPEEDS_COL);
    ++offset;

    printf(screen, "%d: 0 ", TRAINSET_TRAINS[i]);
  }

  restore_cursor(screen);
}

void terminal_screen_init(struct TerminalScreen *screen, int console_tx) {
  screen->console_tx = console_tx;

  puts(screen, SEQ_CLEAR_SCREEN);
  puts(screen, SEQ_CURSOR_HIDE);

  terminal_init_switch_states(screen);
  terminal_init_train_speeds(screen);
  terminal_update_sensors(screen, NULL, 0);
  terminal_update_command(screen, "", 0);
  terminal_update_max_sensor_duration(screen, 0);
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

void terminal_update_switch_state(
    struct TerminalScreen *screen,
    int switch_num,
    enum SwitchDirection dir) {
  save_cursor(screen);

  int switch_index = switch_num;
  if (switch_index >= 153) {
    switch_index -= 134;
  }

  switch_index -= 1;

  int row_offset = switch_index % 9;
  int col_offset = switch_index / 9;

  move_cursor(screen, SWITCHES_ROW + row_offset, col_offset * 10);
  printf(screen, "%d: %c", switch_num, get_switch_dir_char(dir));
  restore_cursor(screen);
}

void terminal_update_time(struct TerminalScreen *screen, uint64_t time) {
  // guaranteed to be less than 64 bits
  int hours = time / HOUR_IN_CENTISECONDS;
  int minutes = (time % HOUR_IN_CENTISECONDS) / MINUTE_IN_CENTISECONDS;
  int seconds = (time % MINUTE_IN_CENTISECONDS) / SECOND_IN_CENTISECONDS;
  int deciseconds = (time % SECOND_IN_CENTISECONDS) / DECISECOND_IN_CENTISECONDS;

  save_cursor(screen);
  puts(screen, SEQ_CURSOR_MOVE_TOP_LEFT);
  printf(screen, "\033[37mCurrent Uptime: %d:%d:%d:%d    ", hours, minutes, seconds, deciseconds);
  restore_cursor(screen);
}

void terminal_update_train_speed(
    struct TerminalScreen *screen,
    int train_index,
    uint8_t train_speed) {
  save_cursor(screen);
  move_cursor(screen, TRAIN_SPEEDS_ROW + train_index, TRAIN_SPEEDS_COL);

  // extra space after speed to overwrite previous two digit numbers
  printf(screen, "%d: %d ", TRAINSET_TRAINS[train_index], train_speed);
  restore_cursor(screen);
}

void terminal_update_command(struct TerminalScreen *screen, char *command, unsigned int len) {
  save_cursor(screen);
  move_cursor(screen, 19, 1);
  puts(screen, "> ");
  putl(screen, command, len);
  puts(screen, SEQ_CURSOR_DELETE_LINE);
  restore_cursor(screen);
}

void terminal_update_max_sensor_duration(
    struct TerminalScreen *screen,
    unsigned int max_sensor_query_duration) {
  save_cursor(screen);
  move_cursor(screen, 3, 1);
  printf(screen, "Sensor query duration in ticks (max): %u", max_sensor_query_duration);
  restore_cursor(screen);
}

void terminal_update_sensors(struct TerminalScreen *screen, bool *sensors, size_t sensors_len) {
  save_cursor(screen);
  move_cursor(screen, 16, 1);

  print_title(screen, "Recently triggered sensors:");
  for (unsigned int i = 0; i < sensors_len; ++i) {
    if (sensors[i]) {
      char ch = 'A' + i / 16;
      printf(screen, " %c%d", ch, i % 16 + 1);

      if (i==2||i==3){
        #if VMEASUREMENT
          Send (WhoIs("vmeasurement"),"1",1,NULL,0);
        #else
        #endif
      }
    }
    
  }
  puts(screen, SEQ_CURSOR_DELETE_LINE);

  restore_cursor(screen);
}

#define CENTISECOND_IN_MICROSECONDS 10000u
#define SECOND_IN_MICROSECONDS (CENTISECOND_IN_MICROSECONDS * 100)
#define MINUTE_IN_MICROSECONDS (SECOND_IN_MICROSECONDS * 60)
#define HOUR_IN_MICROSECONDS (MINUTE_IN_MICROSECONDS * 60)

void terminal_update_idle(struct TerminalScreen *screen, uint64_t idle, int idle_pct) {
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
      centiseconds);
}

void terminal_print_loop_distance(struct TerminalScreen *screen, char * begin, char * end, int distance){
  save_cursor(screen);
  move_cursor(screen, DISTANCE_LINE, 1);
  printf(screen, "START FROM %s TO %s distance: %d ", begin, end, distance);
  restore_cursor(screen);
}

void terminal_print_loop_time(struct TerminalScreen *screen, int train, int speed, int time, int velocity){
  save_cursor(screen);
  move_cursor(screen, VELOCITY_BASE_LINE+velocity_offset, 1);
  velocity_offset++;
  printf(screen, "train %d at %d looptime %d velocity %d", train, speed, time, velocity);
  restore_cursor(screen);
}