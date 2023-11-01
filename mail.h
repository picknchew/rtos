#pragma once

#include "stddef.h"
#include "stdint.h"

struct Message {
  struct TaskDescriptor *sender;
  char *msg;
  int msglen;
};

struct MailQueueNode {
  struct Message *val;
  struct MailQueueNode *next;
};

// simple linked list
struct MailQueue {
  struct MailQueueNode *head;
  struct MailQueueNode *tail;
  size_t size;
};

void mail_queue_init(struct MailQueue *mail_queue);
void mail_init(struct Message *mail, struct TaskDescriptor *task);
void mail_queue_add(struct MailQueue *mail_queue, struct MailQueueNode *node);
unsigned int mail_queue_size(struct MailQueue *mail_queue);
struct MailQueueNode *mail_queue_remove(struct MailQueue *mail_queue, struct TaskDescriptor *sender);
struct MailQueueNode *mail_queue_pop(struct MailQueue *mail_queue);
