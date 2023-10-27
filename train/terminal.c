#include "terminal.h"

#include <stdarg.h>
#include <stdbool.h>

#include "../user/io_server.h"
#include "../user/trainset_task.h"
#include "../util.h"

static const char CHAR_DELIMITER = ' ';

#define DECISECOND_IN_CENTISECONDS 10
#define SECOND_IN_CENTISECONDS (DECISECOND_IN_CENTISECONDS * 10)
#define MINUTE_IN_CENTISECONDS (SECOND_IN_CENTISECONDS * 60)
#define HOUR_IN_CENTISECONDS (MINUTE_IN_CENTISECONDS * 60)

static const char SEQ_CLEAR_SCREEN[] = "\033[2J";
static const char SEQ_CURSOR_MOVE_TOP_LEFT[] = "\033[H";
static const char SEQ_CURSOR_HIDE[] = "\033[?25l";
static const char SEQ_CURSOR_DELETE_LINE[] = "\033[K";

const char TEXT_RESET[] = "\033[0m";

static void init_screen(struct Terminal *terminal);

static void putc(struct Terminal *terminal, char ch) {
  Putc(terminal->console_tx, ch);
}

static void putl(struct Terminal *terminal, const char *buf, size_t blen) {
  Putl(terminal->console_tx, (const unsigned char *) buf, blen);
}

static void puts(struct Terminal *terminal, const char *str) {
  putl(terminal, str, strlen(str));
}

void terminal_init(struct Terminal *terminal, int console_rx_tid, int console_tx_tid) {
  terminal->command_len = 0;
  terminal->screen_last_updated = 0;
  terminal->console_rx = console_rx_tid;
  terminal->console_tx = console_tx_tid;

  circular_buffer_init(&terminal->write_buffer);

  init_screen(terminal);
}

static void format_print(struct Terminal *terminal, char *fmt, va_list va) {
  char bf[12];
  char ch;

  while ((ch = *(fmt++))) {
    if (ch != '%') {
      putc(terminal, ch);
    } else {
      ch = *(fmt++);
      switch (ch) {
        case 'u':
          ui2a(va_arg(va, unsigned int), 10, bf);
          puts(terminal, bf);
          break;
        case 'd':
          i2a(va_arg(va, int), bf);
          puts(terminal, bf);
          break;
        case 'x':
          ui2a(va_arg(va, unsigned int), 16, bf);
          puts(terminal, bf);
          break;
        case 'c':
          putc(terminal, va_arg(va, int));
          break;
        case 's':
          puts(terminal, va_arg(va, char *));
          break;
        case '%':
          putc(terminal, ch);
          break;
        case '\0':
          return;
      }
    }
  }
}

static void printf(struct Terminal *terminal, char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  format_print(terminal, fmt, va);
  va_end(va);
}

static char *strtok_r(char *str, char delim, char **saveptr) {
  char *cur = str;

  if (!str) {
    str = *saveptr;
    cur = *saveptr;
  }

  if (!cur || *cur == '\0') {
    return NULL;
  }

  while (*cur) {
    if (*cur == delim) {
      *cur = '\0';
      *saveptr = cur + 1;
      return str;
    }

    ++cur;
  }

  *saveptr = NULL;
  return str;
}

static void move_cursor(struct Terminal *terminal, int r, int c) {
  printf(terminal, "\033[%d;%dH", r, c);
}

static void save_cursor(struct Terminal *terminal) {
  puts(terminal, "\0337");
}

static void restore_cursor(struct Terminal *terminal) {
  puts(terminal, "\0338");
}

void terminal_update_status_va(struct Terminal *terminal, char *fmt, va_list va) {
  save_cursor(terminal);
  move_cursor(terminal, 20, 1);
  puts(terminal, SEQ_CURSOR_DELETE_LINE);
  printf(terminal, "\033[32m");

  format_print(terminal, fmt, va);

  puts(terminal, TEXT_RESET);
  restore_cursor(terminal);
}

void terminal_update_status(struct Terminal *terminal, char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  terminal_update_status_va(terminal, fmt, va);
  va_end(va);
}

#define CENTISECOND_IN_MICROSECONDS 10000u
#define SECOND_IN_MICROSECONDS (CENTISECOND_IN_MICROSECONDS * 100)
#define MINUTE_IN_MICROSECONDS (SECOND_IN_MICROSECONDS * 60)
#define HOUR_IN_MICROSECONDS (MINUTE_IN_MICROSECONDS * 60)

void terminal_update_idle(struct Terminal *terminal, uint64_t idle, int idle_pct) {
  // guaranteed to be less than 64 bits
  unsigned int hours = idle / HOUR_IN_MICROSECONDS;
  unsigned int minutes = (idle % HOUR_IN_MICROSECONDS) / MINUTE_IN_MICROSECONDS;
  unsigned int seconds = (idle % MINUTE_IN_MICROSECONDS) / SECOND_IN_MICROSECONDS;
  unsigned int centiseconds = (idle % SECOND_IN_MICROSECONDS) / CENTISECOND_IN_MICROSECONDS;

  terminal_update_status(
      terminal,
      "idle_task: idle time (%u%% of uptime) %u:%u:%u:%u\r\n",
      idle_pct,
      hours,
      minutes,
      seconds,
      centiseconds);
}

// Executes a command and returns 1 if the quit command is executed,
// otherwise returns 0. Modifies command.
bool terminal_execute_command(struct Terminal *terminal, int train_tid, char *command) {
  char *saveptr = NULL;
  char *command_name = strtok_r(command, CHAR_DELIMITER, &saveptr);

  if (!command_name) {
    terminal_update_status(terminal, "Invalid command!");
    return false;
  }

  if (strcmp("q", command_name)) {
    return true;
  } else if (strcmp("tr", command_name)) {
    char *str_train_number = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_train_number || !is_number(str_train_number)) {
      terminal_update_status(terminal, "Train provided is not a valid train!");
      return false;
    }

    char *str_train_speed = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_train_speed || !is_number(str_train_speed)) {
      terminal_update_status(terminal, "Must provide a valid speed!");
      return false;
    }

    int train_number = atoi(str_train_number);
    int train_speed = atoi(str_train_speed);

    if (!trainset_is_valid_train(train_number)) {
      terminal_update_status(terminal, "Train provided is not a valid train!");
      return false;
    }

    if (train_speed < 0 || train_speed > 14) {
      return false;
    }

    terminal_update_status(terminal, "Changing speed of train!");

    TrainSetSpeed(train_tid, train_number, train_speed);
  } else if (strcmp("rv", command_name)) {
    char *str_train_number = strtok_r(NULL, CHAR_DELIMITER, &saveptr);

    if (!str_train_number || !is_number(str_train_number)) {
      return false;
    }

    int train_number = atoi(str_train_number);

    if (!trainset_is_valid_train(train_number)) {
      terminal_update_status(terminal, "Train provided is not a valid train!");
      return false;
    }

    terminal_update_status(terminal, "Slowing down train...");
    TrainReverse(train_tid, train_number);
  } else if (strcmp("sw", command_name)) {
    char *str_switch_number = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!is_number(str_switch_number)) {
      terminal_update_status(terminal, "Must provide a valid switch number!");
      return false;
    }

    int switch_number = atoi(str_switch_number);
    char *str_switch_direction = strtok_r(NULL, CHAR_DELIMITER, &saveptr);

    if (!str_switch_direction) {
      return false;
    }

    int switch_direction = 0;

    if (strcmp(str_switch_direction, "C")) {
      switch_direction = TRAINSET_DIRECTION_CURVED;
    }

    if (strcmp(str_switch_direction, "S")) {
      switch_direction = TRAINSET_DIRECTION_STRAIGHT;
    }

    if (!switch_direction) {
      terminal_update_status(terminal, "Invalid switch direction provided!");
      return false;
    }

    TrainSetSwitchDir(train_tid, switch_number, switch_direction);
    terminal_update_status(terminal, "Changing direction of switch!");
  } else {
    terminal_update_status(terminal, "Invalid command!");
  }

  return false;
}

void terminal_clear_command_buffer(struct Terminal *terminal) {
  terminal->command_len = 0;
}

void terminal_update_time(struct Terminal *terminal, uint64_t time) {
  // guaranteed to be less than 64 bits
  int hours = time / HOUR_IN_CENTISECONDS;
  int minutes = (time % HOUR_IN_CENTISECONDS) / MINUTE_IN_CENTISECONDS;
  int seconds = (time % MINUTE_IN_CENTISECONDS) / SECOND_IN_CENTISECONDS;
  int deciseconds = (time % SECOND_IN_CENTISECONDS) / DECISECOND_IN_CENTISECONDS;

  save_cursor(terminal);
  puts(terminal, SEQ_CURSOR_MOVE_TOP_LEFT);
  printf(terminal, "\033[37mCurrent Uptime: %d:%d:%d:%d    ", hours, minutes, seconds, deciseconds);
  restore_cursor(terminal);
}

static void print_title(struct Terminal *terminal, char *title) {
  printf(terminal, "\033[36m%s", title);
  puts(terminal, TEXT_RESET);
}

static char get_switch_dir_char(enum SwitchDirection dir) {
  if (dir == DIRECTION_UNKNOWN) {
    return '?';
  }

  if (dir == DIRECTION_STRAIGHT) {
    return 'S';
  }

  return 'C';
}

static const int SWITCHES_TITLE_ROW = 5;
static const int SWITCHES_ROW = 6;

void terminal_update_switch_state(
    struct Terminal *terminal,
    int switch_num,
    enum SwitchDirection dir) {
  save_cursor(terminal);

  int switch_index = switch_num;
  if (switch_index >= 153) {
    switch_index -= 134;
  }

  switch_index -= 1;

  int row_offset = switch_index % 9;
  int col_offset = switch_index / 9;

  move_cursor(terminal, SWITCHES_ROW + row_offset, col_offset * 10);
  printf(terminal, "%d: %c", switch_num, get_switch_dir_char(dir));
  restore_cursor(terminal);
}

void terminal_init_switch_states(struct Terminal *terminal) {
  save_cursor(terminal);
  move_cursor(terminal, SWITCHES_TITLE_ROW, 1);
  print_title(terminal, "Switch States:");

  int offset = 0;
  for (int switch_num = 1; switch_num <= 9; ++switch_num) {
    move_cursor(terminal, SWITCHES_ROW + offset, 0);
    ++offset;

    printf(terminal, "%d: ?", switch_num);
  }

  offset = 0;
  for (int switch_num = 10; switch_num <= 18; ++switch_num) {
    move_cursor(terminal, SWITCHES_ROW + offset, 10);
    ++offset;

    printf(terminal, "%d: ?", switch_num);
  }

  offset = 0;
  for (int switch_num = 153; switch_num <= 156; ++switch_num) {
    move_cursor(terminal, SWITCHES_ROW + offset, 20);
    ++offset;

    printf(terminal, "%d: ?", switch_num);
  }

  restore_cursor(terminal);
}

static const int TRAIN_SPEEDS_TITLE_ROW = 5;
static const int TRAIN_SPEEDS_COL = 40;
static const int TRAIN_SPEEDS_ROW = 6;

void terminal_update_train_speed(struct Terminal *terminal, int train_index, uint8_t train_speed) {
  save_cursor(terminal);
  move_cursor(terminal, TRAIN_SPEEDS_ROW + train_index, TRAIN_SPEEDS_COL);

  // extra space after speed to overwrite previous two digit numbers
  printf(terminal, "%d: %d ", TRAINSET_TRAINS[train_index], train_speed);
  restore_cursor(terminal);
}

void terminal_init_train_speeds(struct Terminal *terminal) {
  save_cursor(terminal);
  move_cursor(terminal, TRAIN_SPEEDS_TITLE_ROW, TRAIN_SPEEDS_COL);
  print_title(terminal, "Train Speeds:");

  int offset = 0;
  for (unsigned int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    move_cursor(terminal, TRAIN_SPEEDS_ROW + offset, TRAIN_SPEEDS_COL);
    ++offset;

    printf(terminal, "%d: 0 ", TRAINSET_TRAINS[i]);
  }

  restore_cursor(terminal);
}

static const char CHAR_COMMAND_END = '\r';
static const char CHAR_BACKSPACE = 8;

bool terminal_handle_keypress(struct Terminal *terminal, int train_tid, char c) {
  if (c == CHAR_COMMAND_END) {
    terminal->command_buffer[terminal->command_len] = '\0';
    bool exit = terminal_execute_command(terminal, train_tid, terminal->command_buffer);
    terminal_clear_command_buffer(terminal);
    terminal_update_command(terminal);
    return exit;
  }

  if (c == CHAR_BACKSPACE && terminal->command_len > 0) {
    --terminal->command_len;
    terminal_update_command(terminal);
    return false;
  }

  // Clear command buffer if command length exceeds buffer size - 1.
  // Last char must be null terminator.
  if (terminal->command_len == BUFFER_SIZE - 1) {
    terminal_clear_command_buffer(terminal);
  }

  terminal->command_buffer[terminal->command_len++] = c;
  terminal_update_command(terminal);
  return false;
}

void terminal_update_command(struct Terminal *terminal) {
  save_cursor(terminal);
  move_cursor(terminal, 19, 1);
  puts(terminal, SEQ_CURSOR_DELETE_LINE);
  puts(terminal, "> ");
  putl(terminal, terminal->command_buffer, terminal->command_len);
  restore_cursor(terminal);
}

void terminal_update_max_sensor_duration(
    struct Terminal *terminal,
    unsigned int max_sensor_query_duration) {
  save_cursor(terminal);
  move_cursor(terminal, 3, 1);
  printf(terminal, "Sensor query duration in ticks (max): %u", max_sensor_query_duration);
  restore_cursor(terminal);
}

void terminal_update_sensors(struct Terminal *terminal, bool *sensors, size_t sensors_len) {
  save_cursor(terminal);
  move_cursor(terminal, 16, 1);

  print_title(terminal, "Recently triggered sensors:");
  puts(terminal, SEQ_CURSOR_DELETE_LINE);
  for (unsigned int i = 0; i < sensors_len; ++i) {
    if (sensors[i]) {
      char ch = 'A' + i / 16;
      printf(terminal, " %c%d", ch, i % 16 + 1);
    }
  }

  restore_cursor(terminal);
}

static void init_screen(struct Terminal *terminal) {
  puts(terminal, SEQ_CLEAR_SCREEN);

  terminal_init_switch_states(terminal);
  terminal_init_train_speeds(terminal);
  terminal_update_sensors(terminal, NULL, 0);
  terminal_update_command(terminal);
  terminal_update_max_sensor_duration(terminal, 0);
}
