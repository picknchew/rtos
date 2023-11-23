#include "terminal_task.h"

#include <stdarg.h>

#include "shell_view.h"
#include "syscall.h"
#include "terminal.h"
#include "terminal_screen.h"
#include "user/server/clock_server.h"
#include "user/server/io_server.h"
#include "user/server/name_server.h"
#include "user/train/train_calibrator.h"

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

  int console_tx = WhoIs("console_io_tx");

  struct TerminalScreen screen;
  struct TerminalView shell_view = shell_view_create();
  terminal_screen_init(&screen, console_tx, shell_view);

  int tid;
  struct TerminalRequest req;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case UPDATE_TRAIN_SPEED:
        Reply(tid, NULL, 0);
        terminal_update_train_speed(
            &screen, req.update_train_speed_req.train_index, req.update_train_speed_req.train_speed
        );
        break;
      case UPDATE_SENSORS:
        terminal_update_sensors(
            &screen, req.update_sensors_req.sensors, req.update_sensors_req.sensors_len
        );
        Reply(tid, NULL, 0);
        break;
      case UPDATE_STATUS:
        terminal_update_status_va(&screen, req.update_status_req.fmt, req.update_status_req.va);
        Reply(tid, NULL, 0);
        break;
      case LOG_PRINT:
        terminal_log_print_va(&screen, req.log_print_req.fmt, req.log_print_req.va);
        Reply(tid, NULL, 0);
        break;
      case UPDATE_SWITCH_STATE:
        terminal_update_switch_state(
            &screen, req.update_switch_state_req.switch_num, req.update_switch_state_req.dir
        );
        Reply(tid, NULL, 0);
        break;
      case UPDATE_MAX_SENSOR_DURATION:
        Reply(tid, NULL, 0);
        terminal_update_max_sensor_duration(&screen, req.update_max_sensor_duration_req.duration);
        break;
      case UPDATE_IDLE:
        Reply(tid, NULL, 0);
        terminal_update_idle(
            &screen,
            req.update_idle_req.idle,
            req.update_idle_req.idle_pct,
            req.update_idle_req.recent_idle_pct
        );
        break;
      case UPDATE_COMMAND:
        terminal_update_command(
            &screen, req.update_command_req.command, req.update_command_req.len
        );
        Reply(tid, NULL, 0);
        break;
      case UPDATE_TRAIN_INFO:
        terminal_update_train_info(
            &screen,
            req.update_train_info_req.train_num,
            req.update_train_info_req.pos_node,
            req.update_train_info_req.pos_offset,
            req.update_train_info_req.state,
            req.update_train_info_req.next_sensor,
            req.update_train_info_req.sensor_estimate,
            req.update_train_info_req.dest
        );
        Reply(tid, NULL, 0);
        break;
      case TERMINAL_TIME_NOTIFY:
        terminal_update_time(&screen, req.time);
        Reply(tid, NULL, 0);
        break;
      case TERMINAL_DISTANCE:
        terminal_print_loop_distance(
            &screen,
            req.update_distance_req.begin,
            req.update_distance_req.end,
            req.update_distance_req.distance
        );
        Reply(tid, NULL, 0);
        break;
      case TERMINAL_TIME_LOOP:
        terminal_print_loop_time(
            &screen,
            req.update_velocity_req.train_num,
            req.update_velocity_req.train_speed,
            req.update_velocity_req.loop_time,
            req.update_velocity_req.train_velocity
        );
        Reply(tid, NULL, 0);
        break;
    }
  }
}

void terminal_task() {
  int terminal_screen = WhoIs("terminal");
  int train_tid = WhoIs("train");
  int train_calib_tid = WhoIs("train_calib");
  int train_router_tid = WhoIs("train_router");

  Create(TERMINAL_TASK_PRIORITY, terminal_key_press_task);

  struct Terminal terminal;
  terminal_init(&terminal, terminal_screen);

  int tid;
  char ch;
  while (true) {
    Receive(&tid, (char *) &ch, sizeof(ch));

    if (terminal_handle_keypress(&terminal, train_tid, train_calib_tid, train_router_tid, ch)) {
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
      .update_train_speed_req = {.train_index = train_index, .train_speed = train_speed}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateSensors(int tid, bool *sensors, size_t sensors_len) {
  struct TerminalRequest req = {
      .type = UPDATE_SENSORS, .update_sensors_req = {.sensors = sensors, .sensors_len = sensors_len}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateStatus(int tid, char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  struct TerminalRequest req = {.type = UPDATE_STATUS, .update_status_req = {.fmt = fmt, .va = va}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
  va_end(va);
}

void TerminalLogPrint(int tid, char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  struct TerminalRequest req = {.type = LOG_PRINT, .log_print_req = {.fmt = fmt, .va = va}};
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
  va_end(va);
}

void TerminalUpdateSwitchState(int tid, int switch_num, enum SwitchDirection dir) {
  struct TerminalRequest req = {
      .type = UPDATE_SWITCH_STATE, .update_switch_state_req = {.switch_num = switch_num, .dir = dir}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateIdle(int tid, uint64_t idle, int idle_pct, int recent_idle_pct) {
  struct TerminalRequest req = {
      .type = UPDATE_IDLE,
      .update_idle_req = {.idle = idle, .idle_pct = idle_pct, .recent_idle_pct = recent_idle_pct}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateMaxSensorDuration(int tid, unsigned int duration) {
  struct TerminalRequest req = {
      .type = UPDATE_MAX_SENSOR_DURATION, .update_max_sensor_duration_req = {.duration = duration}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateCommand(int tid, char *command, size_t len) {
  struct TerminalRequest req = {
      .type = UPDATE_COMMAND, .update_command_req = {.command = command, .len = len}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateDistance(int tid, const char *begin, const char *end, int distance) {
  struct TerminalRequest req = {
      .type = TERMINAL_DISTANCE,
      .update_distance_req = {.begin = begin, .end = end, .distance = distance}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateVelocity(
    int tid,
    int train_num,
    int train_speed,
    int loop_time,
    int train_velocity
) {
  struct TerminalRequest req = {
      .type = TERMINAL_TIME_LOOP,
      .update_velocity_req =
          {.train_num = train_num,
           .train_speed = train_speed,
           .loop_time = loop_time,
           .train_velocity = train_velocity}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateTrainInfo(
    int tid,
    int train_num,
    char *pos_node,
    int pos_offset,
    char *state,
    char *next_sensor,
    int sensor_estimate,
    char *dest
) {
  struct TerminalRequest req = {
      .type = UPDATE_TRAIN_INFO,
      .update_train_info_req =
          {.train_num = train_num,
           .pos_node = pos_node,
           .pos_offset = pos_offset,
           .state = state,
           .next_sensor = next_sensor,
           .sensor_estimate = sensor_estimate,
           .dest = dest}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

void TerminalUpdateSelectedTrack(int tid, char track) {
  struct TerminalRequest req = {
      .type = UPDATE_TRAIN_INFO, .update_selected_track_req = {.track = track}
  };
  Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
