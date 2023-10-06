#include "mail.h"

#include <stddef.h>
#include "stdint.h"

#include "task.h"

void mail_queue_init(struct MailQueue *mail_queue) {
  mail_queue->head = NULL;
  mail_queue->tail = NULL;
  mail_queue->size = 0;
}

void mail_init(struct Message *mail, struct TaskDescriptor *task) {
  mail->msg = NULL;
  mail->msglen = 0;
  mail->reply = NULL;
  mail->rplen = 0;
  mail->sender = task;
}

void mail_queue_add(struct MailQueue *mail_queue, struct MailQueueNode *node) {
  node->next = NULL;

  if (mail_queue->tail) {
    mail_queue->tail->next = node;
  } else {
    mail_queue->head = node;
  }

  mail_queue->tail = node;
  ++mail_queue->size;
}

struct MailQueueNode *mail_queue_pop(struct MailQueue *mail_queue) {
  struct MailQueueNode *popped = mail_queue->head;

  mail_queue->head = popped->next;
  --mail_queue->size;

  if (mail_queue->size <= 1) {
    mail_queue->tail = mail_queue->head;
  }

  return popped;
}

struct MailQueueNode *mail_queue_remove(struct MailQueue *mail_queue, struct TaskDescriptor *sender) {
  struct MailQueueNode *cur = mail_queue->head;
  struct MailQueueNode *previous = NULL;

  while (cur != NULL) {
    if (cur->val->sender == sender) {
      break;
    }

    previous = cur;
    cur = cur->next;
  }

  if (!cur) {
    return NULL;
  }

  if (!previous) {
    // first element
    return mail_queue_pop(mail_queue);
  }

  if (cur == mail_queue->tail) {
    mail_queue->tail = previous;
  }

  previous->next = cur->next;
  --mail_queue->size;

  return cur;
}
