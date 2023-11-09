#include "idle_task.h"

#include <stdbool.h>
#include <stdint.h>

#include "irq.h"
#include "rpi.h"
#include "syscall.h"
#include "timer.h"
#include "user/server/name_server.h"
#include "user/terminal/terminal_task.h"

#define CENTISECOND_IN_MICROSECONDS 10000u
#define SECOND_IN_MICROSECONDS (CENTISECOND_IN_MICROSECONDS * 100)
#define MINUTE_IN_MICROSECONDS (SECOND_IN_MICROSECONDS * 60)
#define HOUR_IN_MICROSECONDS (MINUTE_IN_MICROSECONDS * 60)

// 100ms in microseconds
static const unsigned int PRINT_INTERVAL = 100000;

void idle_task() {
  int terminal = WhoIs("terminal");

  uint64_t init_time = timer_get_time();
  uint64_t last_printed = 0;
  uint64_t time_elapsed = 0;
  // exponentially weighted moving average
  uint64_t recent_avg = 0;

  while (true) {
    AwaitEvent(EVENT_TIMER);
    time_elapsed += TIMER_TICK_DURATION;

    uint64_t current_time = timer_get_time() - init_time;
    recent_avg += TIMER_TICK_DURATION * 100 / current_time;
    
    int idle_pct = time_elapsed * 100 / current_time;
    int recent_idle_pct = recent_avg / current_time;

    if (current_time > last_printed + PRINT_INTERVAL) {
      TerminalUpdateIdle(terminal, time_elapsed, idle_pct, recent_avg);
      last_printed = current_time;
    }
  }
}
