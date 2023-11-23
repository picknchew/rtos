#pragma once

#include <stdint.h>

#include "trainset.h"

#define TRAIN_SPEED_MAX 14
#define FIXED_POINT_MULTIPLIER 100000

typedef int64_t FixedPointInt;

extern FixedPointInt TRAINSET_MEASURED_SPEEDS[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
extern int TRAINSET_STOPPING_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
extern int TRAINSET_STOPPING_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
extern int TRAINSET_ACCEL_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
extern int TRAINSET_ACCEL_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
extern int TRAINSET_DECEL_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];

inline FixedPointInt fixed_point_int_from(int64_t val) {
  return val * FIXED_POINT_MULTIPLIER;
}

inline int fixed_point_int_get(int64_t val) {
  return val / FIXED_POINT_MULTIPLIER;
}

int shortmove_get_duration(int train, int speed, int dist);
