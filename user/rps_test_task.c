#include "rps_test_task.h"

#include "../syscall.h"
#include "rps_client.h"
#include "rps_server.h"

void rps_test_task() {
  Create(1, rps_server_task);
  Create(1, rps_client_task);
  Create(1, rps_client_task);
  Create(1, rps_client_task);
  Create(1, rps_client_task);
  Exit();
}