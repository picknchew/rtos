#include "terminal_task.h"

#include "../syscall.h"
#include "../train/terminal.h"
#include "clock_server.h"
#include "io_server.h"
#include "name_server.h"
#include "trainset_task.h"

static const char CHAR_COMMAND_END = '\r';
static const char CHAR_BACKSPACE = 8;

void terminal_key_press_task() {
  int terminal = MyParentTid();
  int console_rx = WhoIs("console_io_rx");

  while (true) {
    char ch = Getc(console_rx);
    struct TerminalRequest req = {.type = TERMINAL_KEY_PRESS_NOTIFY, .ch = ch};
    Send(terminal, (const char *) &req, sizeof(req), NULL, 0);
  }
}

void terminal_time_update_task() {
  int terminal = MyParentTid();
  int clock_server = WhoIs("clock_server");

  while (true) {
    Yield();
    struct TerminalRequest req = {.type = TERMINAL_TIME_NOTIFY, .time = Time(clock_server)};
    Send(terminal, (const char *) &req, sizeof(req), NULL, 0);
  }
}

static bool handle_keypress(struct Terminal *terminal, int train_tid, char c) {
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

  terminal_update_command(terminal);
  return false;
}

void terminal_task() {
  RegisterAs("terminal");

  int console_rx = WhoIs("console_io_rx");
  int console_tx = WhoIs("console_io_tx");

  int train_tid = Create(1, train_task);

  struct Terminal terminal;
  terminal_init(&terminal, train_tid, console_rx, console_tx);

  int tid;
  struct TerminalRequest req;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case UPDATE_TRAIN_SPEEDS:
        terminal_update_train_speeds(&terminal, req.update_train_speeds_req.train_speeds);
        break;
      case UPDATE_SENSORS:
        terminal_update_sensors(
            &terminal, req.update_sensors_req.sensors, req.update_sensors_req.sensors_len);
        break;
      case UPDATE_STATUS:
        terminal_update_status(&terminal, req.update_status_req.status);
        break;
      case UPDATE_SWITCH_STATES:
        terminal_update_switch_states(&terminal, train_tid);
        break;
      case UPDATE_MAX_SENSOR_DURATION:
        terminal_update_max_sensor_duration(&terminal, req.update_max_sensor_duration_req.duration);
        break;
      case TERMINAL_KEY_PRESS_NOTIFY:
        if (handle_keypress(&terminal, train_tid, req.ch)) {
          Exit();
        }
        break;
      case TERMINAL_TIME_NOTIFY:
        terminal_update_time(&terminal, req.time);
    }

    Reply(tid, NULL, 0);
  }

  Exit();
}

void TerminalUpdateTrainSpeeds(int tid, uint8_t *train_speeds) {
  struct TerminalRequest req = {
      .type = UPDATE_TRAIN_SPEEDS, .update_train_speeds_req = {.train_speeds = train_speeds}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateSensors(int tid, bool *sensors, size_t sensors_len) {
  struct TerminalRequest req = {
      .type = UPDATE_TRAIN_SPEEDS,
      .update_sensors_req = {.sensors = sensors, .sensors_len = sensors_len}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateStatus(int tid, char *status) {
  struct TerminalRequest req = {
      .type = UPDATE_TRAIN_SPEEDS, .update_status_req = {.status = status}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateSwitchStates(int tid) {
  struct TerminalRequest req = {.type = UPDATE_SWITCH_STATES};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateMaxSensorDuration(int tid, unsigned int duration) {
  struct TerminalRequest req = {
      .type = UPDATE_TRAIN_SPEEDS, .update_max_sensor_duration_req = {.duration = duration}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
