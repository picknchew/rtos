#pragma once

#include <stdbool.h>

void train_manager_task();
void TrainManagerUpdateSensors(int tid, bool *sensors);
void TrainManagerRouteReturn(int tid, int train1, int train2, char *sensor, char *sensor2);
void TrainManagerRouteOneReturn(int tid, int train, char *sensor);
void TrainManagerRandomlyRoute(int tid, int train1, int train2);
