#include "init_task.h"

#include <stdio.h>

#include "idle_task.h"
#include "server/clock_server.h"
#include "server/io_server.h"
#include "server/name_server.h"
#include "syscall.h"
#include "task.h"
#include "terminal/terminal_task.h"
#include "test/msg_perf_test.h"
#include "test/replay_task.h"
#include "test/rps/rps_test_task.h"
#include "test/testk3.h"
#include "test/test_tasks.h"
#include "timer.h"
#include "train/train_dispatcher.h"

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
  Create(30, name_server_task);
  Create(29, clock_server_task);
  Create(IO_TASK_PRIORITY, io_server_task);
  // // Create(19, train_dispatcher_task);

  Create(TERMINAL_TASK_PRIORITY, terminal_task);
  Create(1, idle_task);
  // Create(1, replay_task);
  // task_print();
  // printf("spin\r\n");
  for (;;) {}  // spin forever when no other tasks are running
}
