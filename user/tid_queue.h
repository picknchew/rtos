#include <stdbool.h>

#include "../task.h"

struct TIDQueueNode {
  int tid;
  struct TIDQueueNode *next;
};

// simple linked list
struct TIDQueue {
  struct TIDQueueNode nodes[TASKS_MAX];
  struct TIDQueueNode *head;
  struct TIDQueueNode *tail;
  unsigned int size;
};

void tid_queue_init(struct TIDQueue *queue);
void tid_queue_add(struct TIDQueue *queue, int tid);
int tid_queue_poll(struct TIDQueue *queue);
int tid_queue_head(struct TIDQueue *queue);
bool tid_queue_empty(struct TIDQueue *queue);
