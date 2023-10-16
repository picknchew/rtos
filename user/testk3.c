#include "testk3.h"

#include "../rpi.h"
#include "../syscall.h"
#include "clock_server.h"
#include "name_server.h"

void first_task() {
  int tid[4];
  struct parameter_req req[4];
  char dump[6];
  int priority[4] = {6, 5, 4, 3};  // 3,4,5,6 smaller is higher
  int delay_interval[4] = {10, 23, 33, 71};
  int num_of_delays[4] = {20, 9, 6, 3};

  Create(10, clock_server_task);

  for (int i = 1; i <= 4; i++) {
    Create(priority[i], clock_client);
  }

  for (int i = 0; i < 4; i++) {
    Receive(&tid[i], dump, sizeof(6));
  }

  for (int i = 0; i < 4; i++) {
    req[i].delay_interval = delay_interval[i];
    req[i].num_of_delays = num_of_delays[i];
    Reply(tid[i], (const char *) &req[i], sizeof(req[i]));
  }
}

void clock_client() {
  // send parameter request
  int pid = MyParentTid();
  char dump[6];
  struct parameter_req req;

  Send(pid, dump, 6, (char *) &req, sizeof(req));

  int num_of_delay = req.num_of_delays;
  int delay_interval = req.delay_interval;
  int clock_server = WhoIs("clock_server");

  for (int i = 1; i <= num_of_delay; i++) {
    printf("num_of_delay: %d\tdelay_interval: %d\n", i, delay_interval);
    Delay(clock_server, delay_interval);
  }
}