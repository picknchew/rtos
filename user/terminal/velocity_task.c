#include "velocity_task.h"

#include <stdio.h>

#include "../train/trackdata/distance.h"
#include "terminal_task.h"
#include "util.h"
#include "../server/name_server.h"
#include "../../syscall.h"
#include "../server/clock_server.h"
#include "../train/trainset_task.h"
#include "../train/trackdata/track_data.h"
// #include "trainset.h"

const int STRAIGHT = 0x21;
const int CURVED = 0x22;

static int switch_num[] = {14,13,10,9,8,17,16,15};
static int switch_dir[] = {CURVED,STRAIGHT,STRAIGHT,CURVED,
                          CURVED,STRAIGHT,STRAIGHT,CURVED};

void velocity_measurement_task() {
  RegisterAs("vmeasurement");
  int tid;
  int train_tid = WhoIs("train");
  int terminal_tid = WhoIs("terminal");

  // velocity measurement switch initilization - put train in a loop
  for (int i = 0; i < 8; i++) {
    TrainSetSwitchDir(train_tid,switch_num[i],switch_dir[i]);
  }

  struct TrackNode *track;
  tracka_init(track);
  struct TrackDistance info = track_distance(track, track[2]);
  TerminalUpdateDistance(terminal_tid, info.begin, info.end, info.distance);
  char msg[2];
  int msglen = 0;
  char speed_command[10];
  int t = 0;
  int time_tid = WhoIs("clock_server");
  for (int k = 6; k < 7; k++) {
    for (int m = 12; m < 13; m++) {      
      TrainSetSpeed(train_tid,TRAINSET_TRAINS[k],m);
      for (int i = 0; i < 20; i++) {
        Receive(&tid, msg, 1);
        Reply(tid, NULL, 0);
        int t1 = Time(time_tid);
        Receive(&tid, msg, 1);
        Reply(tid, NULL, 0);
        int t2 = Time(time_tid);
        int loopt = t2-t1;
        if (i >= 2) {
          t += loopt;
        }
        TerminalUpdateVelocity(terminal_tid,TRAINSET_TRAINS[k],m,loopt,info.distance/loopt);
      }
    }
  }
  Exit();
}