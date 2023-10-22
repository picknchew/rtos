#include "init_task.h"

#include <stdio.h>

#include "../syscall.h"
#include "../task.h"
#include "../timer.h"
#include "clock_server.h"
#include "idle_task.h"
#include "io_server.h"
#include "msg_perf_test.h"
#include "name_server.h"
#include "replay_task.h"
#include "rps_test_task.h"
#include "test_tasks.h"
#include "testk3.h"

void init_task() {
#if BENCHMARK
  Create(63, msg_perf_spawner);
#else
  // Create(10, name_server_task);
  // Create(2, rps_test_task);
  // Create(1, idle_task);

  // Create(10, name_server_task);
  // Create(9, clock_server_task);

  // Create(7, test_clock_server_task);
#endif
  printf("looping\r\n");
  Create(20, io_server_task);
  Create(1, replay_task);
  for (;;) {}  // spin forever when no other tasks are running
}
