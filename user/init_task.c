#include "init_task.h"

#include "../syscall.h"
#include "../task.h"
#include "msg_perf_test.h"
#include "name_server.h"
#include "rps_test_task.h"
#include "test_tasks.h"
#include "testk3.h"

void init_task() {
#if BENCHMARK
  Create(63, msg_perf_spawner);
#else
  // Create(10, name_server_task);
  // Create(2, rps_test_task);
  // Create(7, first_task);
#endif
  for (;;) {
    printf("time: %d\r\n", timer_get_time());
  }  // spin forever when no other tasks are running
}
