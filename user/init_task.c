#include "init_task.h"

#include "test_tasks.h"
#include "../syscall.h"

void init_task() {
  Create(2, test_task);

  for (;;) {} // spin forever when no other tasks are running
}
