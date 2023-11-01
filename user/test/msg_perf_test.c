#include "msg_perf_test.h"

#include <stdint.h>

#include "rpi.h"
#include "syscall.h"
#include "timer.h"

#define RECEIVE_TASK 2
#define SEND_TASK 3

#define TEST_BYTES BENCHMARK_MSG_SIZE

static uint64_t start_time = 0;
static uint64_t end_time = 0;

#define BENCHMARK_N 100000

void msg_perf_test() {
  char test_data[TEST_BYTES];
  char dump[TEST_BYTES];

  // let spawner task exit
  Yield();

  start_time = timer_get_time();
  for (int i = 0; i < BENCHMARK_N; ++i) {
// receive first
#if BENCHMARK_TYPE == 1
    Yield();
#endif
    Send(RECEIVE_TASK, test_data, TEST_BYTES, dump, TEST_BYTES);
  }
  end_time = timer_get_time();
  printf(
      "msg_perf: measured time (us) for %d iterations and msg size %d: %u\n",
      BENCHMARK_N,
      TEST_BYTES,
      end_time - start_time);

  Exit();
}

static int tid_dump;

void msg_perf_server() {
  char test_data[TEST_BYTES];
  char dump[TEST_BYTES];

  // msg_perf_server starts first
  Yield();
  // spawn send task
  Yield();
  for (int i = 0; i < BENCHMARK_N; ++i) {
// send-first
#if BENCHMARK_TYPE == 0
    Yield();
#endif
    Receive(&tid_dump, dump, TEST_BYTES);
    Reply(SEND_TASK, test_data, TEST_BYTES);
  }

  Exit();
}

void msg_perf_spawner() {
#if BENCHMARK_TYPE == 0
  printf("msg_perf: running send-first benchmarks\r\n");
#elif BENCHMARK_TYPE == 1
  printf("msg_perf: running receive-first benchmarks\r\n");
#endif

  // ran at one priority lower so that spawner task can exit
  Create(63, msg_perf_server);
  Create(63, msg_perf_test);
  Exit();
}
