#pragma once

#include <stdbool.h>

#include "train_planner.h"

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

void zones_a_init();
void zones_b_init();

// returns false if track could not be reserved.
bool ReserveTrack(int terminal, int zone_num, int train_index);
bool ReservePath(
    int terminal,
    struct RoutePlan *plan,
    struct SimplePath *path,
    int start_node_index,
    int train_index
);
bool ReservableTrack(int zone_num, int train_index);
void ReleaseReservations(int terminal, int zone);
struct Zone *GetZone(int zone);
int ZoneOccupied(int zone);
