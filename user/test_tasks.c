#include "test_tasks.h"

#include "../rpi.h"
#include "../syscall.h"

void print_tid_task() {
  printf("Running task %d, parent id: %d\r\n", MyTid(), MyParentTid());
  Yield();
  printf("Running task %d, parent id: %d\r\n", MyTid(), MyParentTid());
  Exit();
}

void test_task() {
  int task1_id = Create(1, print_tid_task);
  printf("Created: %d\r\n", task1_id);
  int task2_id = Create(1, print_tid_task);
  printf("Created: %d\r\n", task2_id);
  int task3_id = Create(3, print_tid_task);
  printf("Created: %d\r\n", task3_id);
  int task4_id = Create(3, print_tid_task);
  printf("Created: %d\r\n", task4_id);

  puts("FirstUserTask: exiting\r\n");
  Exit();
}
