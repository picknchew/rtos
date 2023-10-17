#include "idle_task.h"

#include <stdbool.h>
#include <stdint.h>

#include "../irq.h"
#include "../syscall.h"
#include "../timer.h"

#define CENTISECOND_IN_MICROSECONDS 100000
#define SECOND_IN_MICROSECONDS (CENTISECOND_IN_MICROSECONDS * 10)
#define MINUTE_IN_MICROSECONDS (SECOND_IN_MICROSECONDS * 60)
#define HOUR_IN_MICROSECONDS (MINUTE_IN_MICROSECONDS * 60)

// 2 seconds in microseconds
static const PRINT_INTERVAL = 2e6;

void idle_task() {
  uint64_t init_time = timer_get_time();
  uint64_t last_printed = 0;
  uint64_t time_elapsed = 0;

  while (true) {
    AwaitEvent(EVENT_TIMER);
    time_elapsed += TIMER_TICK_DURATION;

    uint64_t current_time = timer_get_time() - init_time;
    int idle_pct = time_elapsed * 100 / current_time;

    if (current_time > last_printed + PRINT_INTERVAL) {
      unsigned int hours = current_time / HOUR_IN_MICROSECONDS;
      unsigned int minutes = (current_time % HOUR_IN_MICROSECONDS) / MINUTE_IN_MICROSECONDS;
      unsigned int seconds = (current_time % MINUTE_IN_MICROSECONDS) / SECOND_IN_MICROSECONDS;
      unsigned int centiseconds =
          (current_time % SECOND_IN_MICROSECONDS) / CENTISECOND_IN_MICROSECONDS;

      printf(
          "idle_task: idle time %u:%u:%u:%u (%u%% of uptime)\r\n",
          hours,
          minutes,
          seconds,
          centiseconds,
          idle_pct);
      last_printed = current_time;
    }
  }
}
