#include "clock_server.h"

int clock_server_task() {
  // TODO
}

int Time(int tid) {
  if (tid < 0) {
    return -1;
  }

  // TODO
}

int Delay(int tid, int ticks) {
  if (ticks < 0) {
    return -2;
  }

  if (tid < 0) {
    return -1;
  }

  // TODO

  return 0;
}

int DelayUntil(int tid, int ticks) {
  if (ticks < 0) {
    return -2;
  }

  if (tid < 0) {
    return -1;
  }

  // TODO

  return 0;
}
