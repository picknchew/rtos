#include "clock_server.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../irq.h"
#include "../syscall.h"
#include "../task.h"
#include "name_server.h"

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
  struct ClockServerRequest msg = {.req_type = CLOCK_SERVER_NOTIFY};
  int time = AwaitEvent(EVENT_TIMER);

  Send(clock_server_tid, (const char *) &msg, sizeof(msg), (char *) &time, sizeof(time));
  Exit();
}

void delay_queue_init() {
  queue.head = NULL;
  queue.tail = NULL;
  queue.size = 0;
}

void delay_queue_insert(int tid, int time) {
  struct DelayQueueNode *node = &delay_queue_nodes[tid];

  node->delay = time;

  if (queue.head == NULL) {
    queue.head = node;
    queue.tail = node;
    node->next = NULL;
  } else {
    // insert the node in sorted order by time
    struct DelayQueueNode *cur = queue.head;

    // find correct position
    while (cur != NULL && cur->delay <= node->delay) {
      cur = cur->next;
    }

    struct DelayQueueNode *next = cur->next;
    cur->next = node;
    node->next = next;

    // tail
    if (next == NULL) {
      queue.tail = node;
    }
  }

  ++queue.size;
}

struct DelayQueueNode *delay_queue_pop() {
  struct DelayQueueNode *popped = queue.head;

  queue.head = popped->next;
  --queue.size;

  if (queue.size <= 1) {
    queue.tail = queue.head;
  }

  return popped;
}

struct DelayQueueNode *delay_queue_peek() {
  return queue.head;
}

void clock_server_task() {
  clock_server_tid = MyTid();

  delay_queues_init();
  delay_queue_init();

  // max priority
  Create(63, clock_notifier_task);
  RegisterAs("clock_server");
  printf("clock_server: started with id %d\r\n", MyTid());

  int time = 0;

  int tid;
  struct ClockServerRequest req;

  while (true) {
    printf("clockserver is waiting to receive\r\n");  // printed
    Receive(&tid, (char *) &req, sizeof(req));
    printf("clockserver received notifier");  // printed

    switch (req.req_type) {
      case CLOCK_SERVER_TIME:
        Reply(tid, (const char *) &time, sizeof(time));
        break;
      case CLOCK_SERVER_NOTIFY:
        // update timer
        ++time;

        printf("1\r\n");
        Reply(tid, (const char *) &time, sizeof(time));  // unblock notifier
        // reply to all scheduled tasks whose delays that have passed
        while (queue.size > 0 && delay_queue_peek()->delay <= time) {
          Reply(delay_queue_pop()->tid, (const char *) &time, sizeof(time));
        }
        printf("2\r\n");
        break;
      case CLOCK_SERVER_DELAY:
        // turn delay to delay until
        req.ticks += time;
        // fall through
      case CLOCK_SERVER_DELAY_UNTIL:
        if (req.ticks <= time) {
          // reply instantly
          Reply(tid, (const char *) &time, sizeof(time));
          break;
        }

        delay_queue_insert(tid, req.ticks);
    }
  }

  Exit();
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
