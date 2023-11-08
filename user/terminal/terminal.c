#include "terminal.h"

#include <stdarg.h>
#include <stdbool.h>

#include "terminal_task.h"
#include "user/server/io_server.h"
#include "user/train/train_calibrator.h"
#include "user/train/trainset_task.h"
#include "util.h"

static const char CHAR_DELIMITER = ' ';
static const char CHAR_COMMAND_END = '\r';
static const char CHAR_BACKSPACE = 8;

void terminal_init(struct Terminal *terminal, int screen_tid) {
  terminal->command_len = 0;
  terminal->screen_tid = screen_tid;
}

// Executes a command and returns 1 if the quit command is executed,
// otherwise returns 0. Modifies command.
bool terminal_execute_command(
    struct Terminal *terminal,
    int train_tid,
    int train_calib_tid,
    int train_router_tid,
    char *command
) {
  char *saveptr = NULL;
  char *command_name = strtok_r(command, CHAR_DELIMITER, &saveptr);

  if (!command_name) {
    TerminalUpdateStatus(terminal->screen_tid, "Invalid command!");
    return false;
  }

  if (strcmp("q", command_name)) {
    return true;
  } else if (strcmp("tr", command_name)) {
    char *str_train_number = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_train_number || !is_number(str_train_number)) {
      TerminalUpdateStatus(terminal->screen_tid, "Train provided is not a valid train!");
      return false;
    }

    char *str_train_speed = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_train_speed || !is_number(str_train_speed)) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid speed!");
      return false;
    }

    int train_number = atoi(str_train_number);
    int train_speed = atoi(str_train_speed);

    if (!trainset_is_valid_train(train_number)) {
      TerminalUpdateStatus(terminal->screen_tid, "Train provided is not a valid train!");
      return false;
    }

    if (train_speed < 0 || train_speed > 14) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid speed!");
      return false;
    }

    TerminalUpdateStatus(terminal->screen_tid, "Changing speed of train!");

    TrainSetSpeed(train_tid, train_number, train_speed);
  } else if (strcmp("rv", command_name)) {
    char *str_train_number = strtok_r(NULL, CHAR_DELIMITER, &saveptr);

    if (!str_train_number || !is_number(str_train_number)) {
      return false;
    }

    int train_number = atoi(str_train_number);

    if (!trainset_is_valid_train(train_number)) {
      TerminalUpdateStatus(terminal->screen_tid, "Train provided is not a valid train!");
      return false;
    }

    TerminalUpdateStatus(terminal->screen_tid, "Slowing down train...");
    TrainReverse(train_tid, train_number);
  } else if (strcmp("sw", command_name)) {
    char *str_switch_number = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!is_number(str_switch_number)) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid switch number!");
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
      TerminalUpdateStatus(terminal->screen_tid, "Invalid switch direction provided!");
      return false;
    }

    TrainSetSwitchDir(train_tid, switch_number, switch_direction);
    TerminalUpdateStatus(terminal->screen_tid, "Changing direction of switch!");
  } else if (strcmp("calib", command_name)) {
    char *str_train_number = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_train_number || !is_number(str_train_number)) {
      TerminalUpdateStatus(terminal->screen_tid, "Train provided is not a valid train!");
      return false;
    }

    char *str_train_speed = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_train_speed || !is_number(str_train_speed)) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid speed!");
      return false;
    }

    int train_number = atoi(str_train_number);
    int train_speed = atoi(str_train_speed);

    if (!trainset_is_valid_train(train_number)) {
      TerminalUpdateStatus(terminal->screen_tid, "Train provided is not a valid train!");
      return false;
    }

    if (train_speed < 0 || train_speed > 14) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid speed!");
      return false;
    }

    TerminalUpdateStatus(
        terminal->screen_tid,
        "Beginning calibration of train %d at speed %d",
        train_number,
        train_speed
    );

    TrainCalibratorBeginCalibration(train_calib_tid, train_number, train_speed);
  } else if (strcmp("rt", command_name)) {
    char *str_train_number = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_train_number || !is_number(str_train_number)) {
      TerminalUpdateStatus(terminal->screen_tid, "Train provided is not a valid train!");
      return false;
    }

    char *str_train_speed = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_train_speed || !is_number(str_train_speed)) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid speed!");
      return false;
    }

    char *str_dest_sensor = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_dest_sensor || strlen(str_dest_sensor) > 3) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid sensor!");
      return false;
    }
    // TODO: better error checking for dest sensor

    char *str_dest_offset = strtok_r(NULL, CHAR_DELIMITER, &saveptr);
    if (!str_dest_offset || !is_number(str_dest_offset)) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid offset!");
    }

    int train_number = atoi(str_train_number);
    int train_speed = atoi(str_train_speed);

    int dest_offset = atoi(str_dest_offset);

    if (!trainset_is_valid_train(train_number)) {
      TerminalUpdateStatus(terminal->screen_tid, "Train provided is not a valid train!");
      return false;
    }

    if (train_speed < 0 || train_speed > 14) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid speed!");
      return false;
    }

    if (dest_offset < 0) {
      TerminalUpdateStatus(terminal->screen_tid, "Must provide a valid sensor offset!");
      return false;
    }
    TerminalUpdateStatus(
        terminal->screen_tid,
        "Beginning routing of train %d at speed %d to sensor %s with offset %d",
        train_number,
        train_speed,
        str_dest_sensor,
        dest_offset
    );

    TrainRouterRouteTrain(
        train_router_tid, train_number, train_speed, str_dest_sensor, dest_offset
    );
  } else {
    TerminalUpdateStatus(terminal->screen_tid, "Invalid command!");
  }

  return false;
}

void terminal_clear_command_buffer(struct Terminal *terminal) {
  terminal->command_len = 0;
}

bool terminal_handle_keypress(
    struct Terminal *terminal,
    int train_tid,
    int train_calib_tid,
    int train_router_tid,
    char c
) {
  if (c == CHAR_COMMAND_END) {
    terminal->command_buffer[terminal->command_len] = '\0';
    bool exit = terminal_execute_command(
        terminal, train_tid, train_calib_tid, train_router_tid, terminal->command_buffer
    );
    terminal_clear_command_buffer(terminal);
    TerminalUpdateCommand(terminal->screen_tid, terminal->command_buffer, terminal->command_len);
    return exit;
  }

  if (c == CHAR_BACKSPACE && terminal->command_len > 0) {
    --terminal->command_len;
    TerminalUpdateCommand(terminal->screen_tid, terminal->command_buffer, terminal->command_len);
    return false;
  }

  // Clear command buffer if command length exceeds buffer size - 1.
  // Last char must be null terminator.
  if (terminal->command_len == COMMAND_BUFFER_SIZE - 1) {
    terminal_clear_command_buffer(terminal);
  }

  terminal->command_buffer[terminal->command_len++] = c;
  TerminalUpdateCommand(terminal->screen_tid, terminal->command_buffer, terminal->command_len);
  return false;
}
