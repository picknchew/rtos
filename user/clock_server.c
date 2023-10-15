#include "clock_server.h"
#include <stdbool.h>
#include <stddef.h>
#include "../syscall.h"

static int clock_server_tid = -1;
static struct DelayQueue *queue;

static void ClockNotifier() {
  int n;
  struct ClockServerRequest msg;
  msg.req_type = CLOCK_SERVER_NOTIFT;
  // TODO: AwaitEvent
  while ((n = AwaitEvent(CLOCK_INTERRUPT))) {
    Send(clock_server_tid, &msg, sizeof(msg), &n, sizeof(n));
  }
}

void delay_queue_init() {
  queue->head = NULL;
  queue->tail = NULL;
}

void clock_server_task() {
  // TODO
  int tid;
  struct ClockServerRequest req;
  int response;

  clock_server_tid = MyTid();
  Create(63, ClockNotifier);
  RegisterAs("clock_server");
  printf("clock_server: started with id %d\r\n", MyTid());

  delay_queue_init();

  int timer = 0;  
  while (true){
    Receive(&tid, (char *) &req, sizeof(req));
    switch (req.req_type){
      case CLOCK_SERVER_TIME:
        response = timer;
        Reply(tid, &response, sizeof(response));
        break;
      case CLOCK_SERVER_NOTIFT:
        // update timer
        response = 0;
        Reply(tid, &response, sizeof(response));  //unblock notifier
        timer++;
        // check delay queue
        struct DelayQueueNode *temp = queue->head;
        struct DelayQueueNode *last = NULL;
        while (temp!=NULL&&temp->delay<=timer){
          Reply(temp->tid, &timer, sizeof(timer));
          last = temp;
          temp = temp->next;
        }if (last!=NULL&&temp!=NULL){
          // pop all the node before
          last->next = NULL;
          queue->head = temp;
        }else if (last!=NULL&&temp==NULL){
          queue->head = NULL;
          queue->tail = NULL;
        }
        break;
      case CLOCK_SERVER_DELAY:
        // turn delay to delay until
        req.ticks+=timer;
      case CLOCK_SERVER_DELAY_UNTIL:
        if (req.ticks <= timer) {
          Reply(tid, &timer, sizeof(timer));
        }else {
          struct DelayQueueNode *node;
          node->tid = tid;
          node->delay = req.ticks;
          if (queue->head==NULL){
            queue->head = node;
            queue->tail = node;
            node->next = NULL;
          }else {
            // insert the node to delay queue by delay until time
            struct DelayQueueNode *temp = queue->head;
            struct DelayQueueNode *last = NULL;
            while((temp!=NULL)&&(temp->delay<=node->delay)){
              last = temp;
              temp = temp->next;
            }
            if (last==NULL){
              node->next = queue->head;
              queue->head = node;
            }else if (temp==NULL){
              last->next = node;
              queue->tail = node;
              node->next = NULL;
            }else {
              node->next = temp;
              last->next = node;
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
