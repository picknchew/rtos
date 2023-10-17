#include "testk3.h"

#include "../rpi.h"
#include "../syscall.h"
#include "../timer.h"
#include "clock_server.h"
#include "name_server.h"

void test_client();

struct TestResponse {
  int delay;
  int num_delays;
};

void test_clock_server_task() {
  int priority[4] = {3, 4, 5, 6};
  int delays[4] = {10, 23, 33, 71};
  int num_delays[4] = {20, 9, 6, 3};

  for (int i = 0; i < 2; i++) {
    Create(priority[i], test_client);
  }

  int my_tid = MyTid();
  int rcv_tid;

  while (true) {
    Receive(&rcv_tid, NULL, 0);
    struct TestResponse res = {
        .delay = delays[rcv_tid - my_tid - 1], .num_delays = num_delays[rcv_tid - my_tid - 1]};
    Reply(rcv_tid, (const char *) &res, sizeof(res));
  }

  Exit();
}

void test_client() {
  int pid = MyParentTid();
  int task_tid = MyTid();
  int clock_server = WhoIs("clock_server");

  struct TestResponse res;
  Send(pid, NULL, 0, (char *) &res, sizeof(res));

  for (int i = 1; i <= res.num_delays; i++) {
    Delay(clock_server, res.delay);
    printf(
        "tid: %d, number of delays: %d, delay interval: %d time: %d\r\n",
        task_tid,
        res.num_delays,
        res.delay,
        timer_get_time() / TIMER_TICK_DURATION);
  }

  Exit();
}