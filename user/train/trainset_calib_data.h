#pragma once

#include "trainset.h"

#define TRAIN_SPEED_MAX 14
#define FIXED_POINT_MULTIPLIER 100

extern FixedPointInt TRAINSET_MEASURED_SPEEDS[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
extern int TRAINSET_STOPPING_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
extern int TRAINSET_STOPPING_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
extern int TRAINSET_ACCEL_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
extern int TRAINSET_DECEL_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];

typedef int FixedPointInt;

inline FixedPointInt fixed_point_int_from(int val) {
  return val * FIXED_POINT_MULTIPLIER;
}

inline int fixed_point_int_get(int val) {
  return val / FIXED_POINT_MULTIPLIER;
}
