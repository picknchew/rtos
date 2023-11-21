#include "trainset_calib_data.h"

// add 1 to include 0
FixedPointInt TRAINSET_MEASURED_SPEEDS[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_STOPPING_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_STOPPING_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_ACCEL_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_ACCEL_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_DECEL_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];

void trainset_calib_data_init() {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    TRAINSET_MEASURED_SPEEDS[i][0] = 0;
    TRAINSET_STOPPING_DISTANCES[i][0] = 0;
    TRAINSET_STOPPING_TIMES[i][0] = 0;
    // default 3s
    TRAINSET_ACCEL_TIMES[i][0] = 300;
    // default 5s
    TRAINSET_DECEL_TIMES[i][0] = 500;
  }

  // mm/s
  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(77)][14] = 185;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(77)][14] = 0;

  // TRAIN 58
  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(58)][13] = 510;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(58)][13] = 980;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(58)][10] = 308;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(58)][10] = 440;
  TRAINSET_ACCEL_DISTANCES[trainset_get_train_index(58)][10] = 289;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(58)][7] = 334;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(58)][7] = 145;

  // TRAIN 54
  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][13] = 599;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][13] = 940;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][12] = 597;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][12] = 955;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][12] = 585;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][12] = 860;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][11] = 537;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][11] = 780;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][10] = 483;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][10] = 705;
  TRAINSET_ACCEL_DISTANCES[trainset_get_train_index(54)][10] = 499;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][9] = 432;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][9] = 635;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][8] = 385;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][8] = 558;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][7] = 315;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][7] = 490;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][6] = 285;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][6] = 400;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][5] = 231;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][5] = 320;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][4] = 174;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][4] = 250;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][3] = 127;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][3] = 160;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(54)][2] = 75;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(54)][2] = 80;

  // TRAIN 47
  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(47)][13] = 566;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(47)][13] = 920;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(47)][10] = 477;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(47)][10] = 885;
  TRAINSET_ACCEL_DISTANCES[trainset_get_train_index(47)][10] = 379;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(47)][7] = 351;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(47)][7] = 615;
}

// first two points are for the line
int interpolate_linear(FixedPointInt x1, int y1, FixedPointInt x2, int y2, int interpolate_x) {
  int slope = fixed_point_int_from(y2 - y1) / (x2 - x1);
  int b = fixed_point_int_from(y1) / (slope * x1);

  return slope * interpolate_x + b;
}

int interpolate(FixedPointInt *dists, FixedPointInt *speeds, int len, int x) {
  int index = 0;
  for (int i = 0; i < len - 1; ++i) {
    if (dists[i + 1] > x) {
      index = i;
      break;
    }
  }

  FixedPointInt x1 = dists[index];
  int y1 = speeds[index];
  FixedPointInt x2 = dists[index + 1];
  int y2 = speeds[index + 2];

  return interpolate_linear(x1, y1, x2, y2, x);
}

// distances in mm scaled by 10, multiply by 10 to get fixed point int in mm
static FixedPointInt train58_dists[] = {69 * 10, 122 * 10, 276 * 10, 560 * 10, 2437 * 10};
static int train58_delays[] = {40, 50, 75, 100, 200};

static FixedPointInt train47_dists[] = {105 * 10, 175 * 10, 528 * 10, 945 * 10, 4576 * 10};
static int train47_delays[] = {40, 50, 75, 100, 200};

int get_shortmove_duration(int train, int speed, int dist) {
  // assume train is speed 10 for now.
  switch (train) {
    case 58:
      return interpolate(train58_dists, train58_delays, 5, dist);
    case 47:
      return interpolate(train47_dists, train47_delays, 5, dist);
  }

  return 0;
}
