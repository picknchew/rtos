#include "terminal_task.h"

#include <stdarg.h>

#include "../syscall.h"
#include "../train/terminal.h"
#include "clock_server.h"
#include "io_server.h"
#include "name_server.h"
#include "trainset_task.h"

const int TERMINAL_TASK_PRIORITY = 3;

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
    // 50ms
    Delay(clock_server, 5);
    struct TerminalRequest req = {.type = TERMINAL_TIME_NOTIFY, .time = Time(clock_server)};
    Send(terminal, (const char *) &req, sizeof(req), NULL, 0);
  }
}

void terminal_task() {
  RegisterAs("terminal");

  int console_rx = WhoIs("console_io_rx");
  int console_tx = WhoIs("console_io_tx");

  int train_tid = Create(TRAIN_TASK_PRIORITY, train_task);

  Create(TERMINAL_TASK_PRIORITY, terminal_key_press_task);
  // Create(TERMINAL_TASK_PRIORITY, terminal_time_update_task);

  struct Terminal terminal;
  terminal_init(&terminal, console_rx, console_tx);

  int tid;
  struct TerminalRequest req;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case UPDATE_TRAIN_SPEED:
        Reply(tid, NULL, 0);
        terminal_update_train_speed(
            &terminal,
            req.update_train_speed_req.train_index,
            req.update_train_speed_req.train_speed);
        break;
      case UPDATE_SENSORS:
        terminal_update_sensors(
            &terminal, req.update_sensors_req.sensors, req.update_sensors_req.sensors_len);
        Reply(tid, NULL, 0);
        break;
      case UPDATE_STATUS:
        terminal_update_status_va(&terminal, req.update_status_req.fmt, req.update_status_req.va);
        Reply(tid, NULL, 0);
        break;
      case UPDATE_SWITCH_STATE:
        terminal_update_switch_state(
            &terminal, req.update_switch_state_req.switch_num, req.update_switch_state_req.dir);
        Reply(tid, NULL, 0);
        break;
      case UPDATE_MAX_SENSOR_DURATION:
        Reply(tid, NULL, 0);
        terminal_update_max_sensor_duration(&terminal, req.update_max_sensor_duration_req.duration);
        break;
      case UPDATE_IDLE:
        Reply(tid, NULL, 0);
        terminal_update_idle(&terminal, req.update_idle_req.idle, req.update_idle_req.idle_pct);
        break;
      case TERMINAL_KEY_PRESS_NOTIFY:
        if (terminal_handle_keypress(&terminal, train_tid, req.ch)) {
          terminal_update_status(&terminal, "Exited.");
          Exit();
        }
        Reply(tid, NULL, 0);
        break;
      case TERMINAL_TIME_NOTIFY:
        terminal_update_time(&terminal, req.time);
        Reply(tid, NULL, 0);
    }
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
