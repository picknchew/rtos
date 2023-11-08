#include <stdbool.h>

void train_router_task();

void TrainRouterRouteTrain(int tid, int train, int speed, char *sensor, int offset);
void TrainRouterUpdateSensors(int tid, bool *sensors);
