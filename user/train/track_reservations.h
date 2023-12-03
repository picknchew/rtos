#pragma once

#include <stdbool.h>

#define ZONE_NUMBERS 34

static const char red[] = "\033[0;31m";
static const char green[] = "\033[0;32m";
static const char yellow[] = "\033[0;33m";
static const char blue[] = "\033[0;34m";
static const char purple[] = "\033[0;35m";
static const char cyan[] = "\033[0;36m";

static const char black[] = "\033[0;30m";

struct Zone {
  int id;
  bool reserved;
  int reservedby;
  int tracks[30];
  int len;
  const char *color;
  int release_counter;
};

// returns false if track could not be reserved.
bool ReserveTrack(int zone_num, int train_index);
bool ReservableTrack(int zone_num, int train_index);
void ReleaseReservations(int zone);
struct Zone getZone(int zone);
int ZoneOccupied(int zone);
