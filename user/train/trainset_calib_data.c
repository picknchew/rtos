#include "trainset_calib_data.h"

// add 1 to include 0
FixedPointInt TRAINSET_MEASURED_SPEEDS[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_STOPPING_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_STOPPING_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_ACCEL_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_ACCEL_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_DECEL_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];

// distances in mm scaled by 10
static FixedPointInt train47_dists[] = {65, 122, 275, 560, 2438, 7000, 11100};
static int train47_delays[] = {40, 50, 75, 100, 200, 300, 400};

static FixedPointInt train58_dists[] = {105, 175, 528, 945, 4576};
static int train58_delays[] = {40, 50, 75, 100, 200};

static FixedPointInt train54_dists[] = {51, 118, 283, 495, 5758, 7790, 9855, 11460, 12500, 17900};
static int train54_delays[] = {40, 50, 75, 100, 300, 325, 350, 375, 400, 500};

static void shortmove_dist_init(FixedPointInt *dists, int len);

void trainset_calib_data_init() {
  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    TRAINSET_MEASURED_SPEEDS[i][0] = 0;
    TRAINSET_STOPPING_DISTANCES[i][0] = 0;
    TRAINSET_STOPPING_TIMES[i][0] = 0;
    // default 3s
    TRAINSET_ACCEL_TIMES[i][0] = 300;
    // default 5s
    TRAINSET_DECEL_TIMES[i][0] = 500;
    TRAINSET_DECEL_TIMES[i][10] = 500;
  }

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(77)][14] = 185;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(77)][14] = 0;

  // TRAIN 58
  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(58)][13] = 510;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(58)][13] = 980;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(58)][10] = 308;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(58)][10] = 440;
  TRAINSET_ACCEL_DISTANCES[trainset_get_train_index(58)][10] = 289;
  TRAINSET_ACCEL_TIMES[trainset_get_train_index(58)][10] = 290;

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
  TRAINSET_ACCEL_TIMES[trainset_get_train_index(54)][10] = 390;

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

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(47)][10] = 360;     // 477
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(47)][10] = 845;  // 885
  TRAINSET_ACCEL_DISTANCES[trainset_get_train_index(47)][10] = 379;     // 379
  TRAINSET_ACCEL_TIMES[trainset_get_train_index(47)][10] = 436;

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(47)][7] = 351;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(47)][7] = 615;

  for (int i = 0; i < TRAINSET_NUM_TRAINS; ++i) {
    for (int j = 0; j < TRAIN_SPEED_MAX + 1; ++j) {
      // measured speeds are scaled by 100. we will scale them so that they are fixed point integers
      TRAINSET_MEASURED_SPEEDS[i][j] *= FIXED_POINT_MULTIPLIER / 100;
    }
  }

  shortmove_dist_init(train58_dists, 5);
  shortmove_dist_init(train54_dists, 10);
  shortmove_dist_init(train47_dists, 7);
}

// first two points are for the line
int interpolate_linear(
    FixedPointInt x1,
    int64_t y1,
    FixedPointInt x2,
    int64_t y2,
    int interpolate_x
) {
  FixedPointInt slope = fixed_point_int_from(fixed_point_int_from(y2 - y1)) / (x2 - x1);
  FixedPointInt b = fixed_point_int_from(y1) - fixed_point_int_get(slope * x1);

  return fixed_point_int_get(slope * interpolate_x + b);
}

int interpolate(FixedPointInt *dists, int *delays, int len, int x) {
  int index = len - 2;
  for (int i = 0; i < len - 1; ++i) {
    if (dists[i + 1] > fixed_point_int_from(x)) {
      index = i;
      break;
    }
  }

  FixedPointInt x1 = dists[index];
  int y1 = delays[index];
  FixedPointInt x2 = dists[index + 1];
  int y2 = delays[index + 1];

  return interpolate_linear(x1, y1, x2, y2, x);
}

static void shortmove_dist_init(FixedPointInt *dists, int len) {
  for (int i = 0; i < len; ++i) {
    // dists are scaled by 10, scale them so they are fixed point ints.
    dists[i] *= FIXED_POINT_MULTIPLIER / 10;
  }
}

int shortmove_get_duration(int train, int speed, int dist) {
  if (dist <= 0) {
    return 0;
  }

  int ret = 0;

  // assume train is speed 10 for now.
  switch (train) {
    case 58:
      ret = interpolate(train58_dists, train58_delays, 5, dist);
      break;
    case 54:
      ret = interpolate(train54_dists, train54_delays, 10, dist);
      break;
    case 47:
      ret = interpolate(train47_dists, train47_delays, 7, dist);
      break;
  }

  if (ret < 0) {
    return 0;
  }

  return ret;
}
