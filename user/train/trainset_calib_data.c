#include "trainset_calib_data.h"

// add 1 to include 0
FixedPointInt TRAINSET_MEASURED_SPEEDS[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_STOPPING_DISTANCES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
int TRAINSET_STOPPING_TIMES[TRAINSET_NUM_TRAINS][TRAIN_SPEED_MAX + 1];
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

  TRAINSET_MEASURED_SPEEDS[trainset_get_train_index(47)][7] = 351;
  TRAINSET_STOPPING_DISTANCES[trainset_get_train_index(47)][7] = 615;
}
