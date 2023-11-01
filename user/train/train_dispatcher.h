#pragma once

void train_dispatcher_task();
int DispatchTrainCommand(int tid, const unsigned char *cmd, unsigned int len);
