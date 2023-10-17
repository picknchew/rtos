#include "clock_server.h"
#include "name_server.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../syscall.h"
#include "../irq.h"
#include "../task.h"

static int clock_server_tid = -1;
static struct DelayQueue queue;

static struct DelayQueueNode delay_queue_nodes[TASKS_MAX];
void delay_queues_init() {
  for (int i = 0; i < TASKS_MAX; ++i) {
    delay_queue_nodes[i].tid = i;
    delay_queue_nodes[i].delay = 0;
    delay_queue_nodes[i].next = NULL;
  }
}


void clock_notifier_task() {
  int n;
  struct ClockServerRequest msg;
  msg.req_type = CLOCK_SERVER_NOTIFY;
  // TODO: AwaitEvent
  printf("clock_notifier_before_while");
  while ((n = AwaitEvent(EVENT_TIMER))) {
    printf("notifier");
    Send(clock_server_tid, (const char *)&msg, sizeof(msg), (char *)&n, sizeof(n));
    printf("notifier finished");
  }
}

void delay_queue_init() {
  queue.head = NULL;
  queue.tail = NULL;
}

void clock_server_task() {
  // TODO
  int tid;
  struct ClockServerRequest req;
  int response;
  delay_queues_init();
  clock_server_tid = MyTid();
  Create(63, clock_notifier_task);
  RegisterAs("clock_server");
  printf("clock_server: started with id %d\r\n", MyTid());

  delay_queue_init();

  int timer = 0;
  while (true) {
    printf("clockserver is waiting to receive\r\n"); // printed
    Receive(&tid, (char *) &req, sizeof(req));
    printf("clockserver received notifier"); //printed
    switch (req.req_type) {
      case CLOCK_SERVER_TIME:
        response = timer;
        Reply(tid, (const char*)&response, sizeof(response));
        break;
      case CLOCK_SERVER_NOTIFY:
        // update timer
        printf("1\r\n");
        response = 0;
        Reply(tid, (const char*)&response, sizeof(response));  // unblock notifier
        timer++;
        // check delay queue
        struct DelayQueueNode *temp = queue.head;
        struct DelayQueueNode *last = NULL;
        while (temp != NULL && temp->delay <= timer) {
          Reply(temp->tid, (const char*)&timer, sizeof(timer));
          last = temp;
          temp = temp->next;
        }
        if (last != NULL && temp != NULL) {
          // pop all the node before
          last->next = NULL;
          queue.head = temp;
        } else if (last != NULL && temp == NULL) {
          queue.head = NULL;
          queue.tail = NULL;
        }
        printf("2\r\n");
        break;
      case CLOCK_SERVER_DELAY:
        // turn delay to delay until
        req.ticks += timer;
      case CLOCK_SERVER_DELAY_UNTIL:
        if (req.ticks <= timer) {
          Reply(tid, (const char*)&timer, sizeof(timer));
        } else {
          struct DelayQueueNode node = delay_queue_nodes[tid];
          node.delay = req.ticks;
          if (queue.head == NULL) {
            queue.head = &node;
            queue.tail = &node;
            node.next = NULL;
          } else {
            // insert the node to delay queue by delay until time
            struct DelayQueueNode *temp = queue.head;
            struct DelayQueueNode *last = NULL;
            while ((temp != NULL) && (temp->delay <= node.delay)) {
              last = temp;
              temp = temp->next;
            }
            if (last == NULL) {
              node.next = queue.head;
              queue.head = &node;
            } else if (temp == NULL) {
              last->next = &node;
              queue.tail = &node;
              node.next = NULL;
            } else {
              node.next = temp;
              last->next = &node;
            }
          }
        }
        break;
    }
  }
}

int Time(int tid) {
  if (tid < 0) {
    return -1;
  }

  // TODO
  struct ClockServerRequest req;
  req.req_type = CLOCK_SERVER_TIME;

  int ticks;
  Send(tid, (const char *) &req, sizeof(req), (char *) &ticks, sizeof(ticks));
  return ticks;
}

int Delay(int tid, int ticks) {
  if (ticks < 0) {
    return -2;
  }

  if (tid < 0) {
    return -1;
  }

  // TODO
  struct ClockServerRequest req;
  req.req_type = CLOCK_SERVER_DELAY;
  req.ticks = ticks;

  int time;
  Send(tid, (const char *) &req, sizeof(req), (char *) &time, sizeof(time));
  return 0;
}

int DelayUntil(int tid, int ticks) {
  if (ticks < 0) {
    return -2;
  }

  if (tid < 0) {
    return -1;
  }

  // TODO
  struct ClockServerRequest req;
  req.req_type = CLOCK_SERVER_DELAY_UNTIL;
  req.ticks = ticks;

  int time;
  Send(tid, (const char *) &req, sizeof(req), (char *) &time, sizeof(time));

  return 0;
}
