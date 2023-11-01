#include "velocity_task.h"

#include <stdio.h>

#include "terminal_task.h"
#include "util.h"
#include "../server/name_server.h"
#include "../../syscall.h"
#include "../server/clock_server.h"

static char *switch_init[8] =
    {"sw 14 C", "sw 13 S", "sw 10 S", "sw 9 C", "sw 8 C", "sw 17 S", "sw 16 S", "sw 15 C"};
static int switchc_len[8] = {7, 7, 7, 6, 6, 7, 7, 7};

// const char *TRAINS[] = {"1", "2", "24", "47", "54", "58", "77", "78"};
// static int trains_len[] = {1, 1, 2, 2, 2, 2, 2, 2};
// const char *TRAINSET_SPEED[] =
//     {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14"};

void velocity_measurement_task() {
  RegisterAs("vmeasurement");
  struct VelocityMeasurementInfo info;
  //   int terminal = MyParentTid();
  int tid;
  Receive(&tid, (char *) &info, sizeof(info));
  struct Terminal *terminal = info.terminal;
  int train_tid = info.train_tid;

  // velocity measurement switch initilization - put train in a loop
  for (int i = 0; i < 8; i++) {
    memcpy(terminal->command_buffer, switch_init[i], switchc_len[i]);
    terminal->command_buffer[switchc_len[i]] = '\0';
    terminal_execute_command(terminal, train_tid, terminal->command_buffer);
    terminal_clear_command_buffer(terminal);
  }

  char msg[2];
  int msglen = 0;
  char speed_command[10];
  int t = 0;
  int time_tid = WhoIs("clock_server");
  for (int k = 0; k < 8; k++) {
    for (int m = 0; m < 15; m++) {
    //   strcat(speed_command, "tr ", 3);
    //   strcat(speed_command, TRAINS[k], trains_len[k]);
    //   strcat(speed_command, " ", 1);
    //   strcat(speed_command, TRAINSET_SPEED[m], m / 10 + 1);
    //   msglen = 4 + trains_len[k] + m / 10 + 1;
    //   memcpy(terminal->command_buffer, speed_command, msglen);
      TrainSetSpeed(train_tid,TRAINSET_TRAINS[k],m);
      for (int i = 0; i < 100; i++) {
        Receive(&tid, msg, 1);
        Reply(tid, NULL, 0);
        int t1 = Time(time_tid);
        Receive(&tid, msg, 1);
        Reply(tid, NULL, 0);
        int t2 = Time(time_tid);
        if (i >= 2) {
          t += t2 - t1;
        }
        printf("train: %d speed:%d time:%d\r\n",k,t,t2-t1);
      }
    }
  }
  Exit();
}