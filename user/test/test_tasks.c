#include "test_tasks.h"

#include "rpi.h"
#include "syscall.h"
#include "user/server/name_server.h"

void print_tid_task() {
  int sender;
  char msg[100];
  printf("Running task %d, parent id: %d\r\n", MyTid(), MyParentTid());
  Yield();
  printf("Receiver receives first\r\n");
  Receive(&sender, msg, 100);
  printf("Reciver reply 'helllllllo' \r\n");
  int rplen = Reply(2, "helllllllo", 11);
  printf("rpl %d \r\n", rplen);

  Exit();
}

void test_task() {
  printf("register task\n");

  printf("after task\n");

  // receive first
  char msg[6] = {'\0'};
  int task1_id = Create(3, print_tid_task);
  printf("Created: %d\r\n", task1_id);

  printf("Sender sends second: 'hello' \r\n");
  Send(3, "hello", 6, msg, 5);
  // msglen should be max arraysize-1

  printf("Sender receives reply %s\r\n", msg);

  // int task2_id = Create(1, print_tid_task);
  // printf("Created: %d\r\n", task2_id);

  // int task3_id = Create(3, print_tid_task);
  // printf("Created: %d\r\n", task3_id);

  // int task4_id = Create(3, print_tid_task);
  // printf("Created: %d\r\n", task4_id);

  // same priority as test_task
  // tid = 2
  int task5_id = Create(2, recv_task);
  printf("Created: recv task, tid = %d\r\n", task5_id);

  //  tid = 3
  int task6_id = Create(3, send_task);
  printf("Created: send task, tid = %d\r\n", task6_id);

  puts("FirstUserTask: exiting\r\n");
  Exit();
}

void send_task() {
  char msg[6] = {'\0'};
  printf("Running task %d, parent id: %d\r\n", MyTid(), MyParentTid());
  printf("Sender sends first 'hello'\r\n");
  Send(3, "hello", 6, msg, 5);
  printf("Sender receives reply %s\r\n", msg);
  Exit();
}

void recv_task() {
  int sender;
  char msg[100];
  // printf("%d",&sender);
  // printf("%d",msg);
  printf("Running task %d, parent id: %d\r\n", MyTid(), MyParentTid());
  printf("Receiver receives second\r\n");
  Receive(&sender, msg, 100);
  // printf("%d,Receiver replys 'helllllllo'\r\n",a);
  int rplen = Reply(4, "helllllllo", 11);
  printf("rpl %d \r\n", rplen);
  Exit();
}
