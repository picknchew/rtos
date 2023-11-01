#include "terminal_task.h"

#include <stdarg.h>

#include "syscall.h"
#include "terminal.h"
#include "terminal_screen.h"
#include "user/server/clock_server.h"
#include "user/server/io_server.h"
#include "user/server/name_server.h"
#include "user/train/trainset_task.h"

const int TERMINAL_TASK_PRIORITY = 3;

void terminal_key_press_task() {
  int terminal = MyParentTid();
  int console_rx = WhoIs("console_io_rx");

  while (true) {
    char ch = Getc(console_rx);
    Send(terminal, (const char *) &ch, sizeof(ch), NULL, 0);
  }
}

void terminal_time_update_task() {
  int terminal = MyParentTid();
  int clock_server = WhoIs("clock_server");

  while (true) {
    // 50ms
    Delay(clock_server, 5);
    struct TerminalRequest req = {.type = TERMINAL_TIME_NOTIFY, .time = Time(clock_server)};
    Send(terminal, (const char *) &req, sizeof(req), NULL, 0);
  }
}

void terminal_screen_task() {
  RegisterAs("terminal");

  Create(TERMINAL_TASK_PRIORITY, terminal_time_update_task);

  struct TerminalScreen screen;
  terminal_screen_init(&screen);

  int tid;
  struct TerminalRequest req;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case UPDATE_TRAIN_SPEED:
        Reply(tid, NULL, 0);
        terminal_update_train_speed(
            &screen,
            req.update_train_speed_req.train_index,
            req.update_train_speed_req.train_speed);
        break;
      case UPDATE_SENSORS:
        terminal_update_status(&screen, "Reversing train..");
        terminal_update_sensors(
            &screen, req.update_sensors_req.sensors, req.update_sensors_req.sensors_len);
        Reply(tid, NULL, 0);
        break;
      case UPDATE_STATUS:
        terminal_update_status_va(&screen, req.update_status_req.fmt, req.update_status_req.va);
        Reply(tid, NULL, 0);
        break;
      case UPDATE_SWITCH_STATE:
        terminal_update_switch_state(
            &screen, req.update_switch_state_req.switch_num, req.update_switch_state_req.dir);
        Reply(tid, NULL, 0);
        break;
      case UPDATE_MAX_SENSOR_DURATION:
        Reply(tid, NULL, 0);
        terminal_update_max_sensor_duration(&screen, req.update_max_sensor_duration_req.duration);
        break;
      case UPDATE_IDLE:
        Reply(tid, NULL, 0);
        terminal_update_idle(&screen, req.update_idle_req.idle, req.update_idle_req.idle_pct);
        break;
      case UPDATE_COMMAND:
        Reply(tid, NULL, 0);
        terminal_update_command(
            &screen, req.update_command_req.command, req.update_command_req.len);
        break;
      case TERMINAL_TIME_NOTIFY:
        terminal_update_time(&screen, req.time);
        Reply(tid, NULL, 0);
        break;
    }
  }
}

void terminal_task() {
  int terminal_screen = Create(TERMINAL_TASK_PRIORITY, terminal_screen_task);

  int train_tid = Create(TRAIN_TASK_PRIORITY, train_task);
  Create(TERMINAL_TASK_PRIORITY, terminal_key_press_task);

  struct Terminal terminal;
  terminal_init(&terminal, terminal_screen);

  int tid;
  char ch;
  while (true) {
    Receive(&tid, (char *) &ch, sizeof(ch));

    if (terminal_handle_keypress(&terminal, train_tid, ch)) {
      TerminalUpdateStatus(terminal_screen, "Exited.");
      Exit();
    }

    Reply(tid, NULL, 0);
  }

  Exit();
}

void TerminalUpdateTrainSpeed(int tid, int train_index, uint8_t train_speed) {
  struct TerminalRequest req = {
      .type = UPDATE_TRAIN_SPEED,
      .update_train_speed_req = {.train_index = train_index, .train_speed = train_speed}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateSensors(int tid, bool *sensors, size_t sensors_len) {
  struct TerminalRequest req = {
      .type = UPDATE_SENSORS,
      .update_sensors_req = {.sensors = sensors, .sensors_len = sensors_len}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateStatus(int tid, char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  struct TerminalRequest req = {.type = UPDATE_STATUS, .update_status_req = {.fmt = fmt, .va = va}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
  va_end(va);
}

void TerminalUpdateSwitchState(int tid, int switch_num, enum SwitchDirection dir) {
  struct TerminalRequest req = {
      .type = UPDATE_SWITCH_STATE,
      .update_switch_state_req = {.switch_num = switch_num, .dir = dir}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateIdle(int tid, uint64_t idle, int idle_pct) {
  struct TerminalRequest req = {
      .type = UPDATE_IDLE, .update_idle_req = {.idle = idle, .idle_pct = idle_pct}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateMaxSensorDuration(int tid, unsigned int duration) {
  struct TerminalRequest req = {
      .type = UPDATE_MAX_SENSOR_DURATION, .update_max_sensor_duration_req = {.duration = duration}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateCommand(int tid, char *command, size_t len) {
  struct TerminalRequest req = {
      .type = UPDATE_MAX_SENSOR_DURATION, .update_command_req = {.command = command, .len = len}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
