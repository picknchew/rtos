#pragma once
#include <stddef.h>
#include <stdint.h>

#include "mail.h"

#define TASKS_MAX 128
#define NUM_REGISTERS 31

#define STACK_SIZE 8192

enum TaskStatus {
  TASK_ACTIVE,
  TASK_READY,
  TASK_EXITED,
  TASK_SEND_BLOCKED,
  TASK_RECEIVE_BLOCKED,
  TASK_REPLY_BLOCKED,
  TASK_EVENT_BLOCKED
};

// must update kern_exit in exceptions.S if the size of this struct changes.
struct TaskContext {
  uint64_t registers[NUM_REGISTERS];
  // SP_EL0
  uint64_t sp;
  // ELR_0 (PC)
  uint64_t lr;
  // SPSR_EL1
  uint64_t pstate;
};

struct Recvbuffer {
  int *tid;
  char *msg;
  int msglen;
};

struct TaskDescriptor {
  uint32_t tid;
  uint32_t priority;

  struct TaskContext context;  // offset: 8

  struct TaskDescriptor *parent;
  enum TaskStatus status;

  // block of memory allocated for this task
  uint64_t stack[STACK_SIZE];

  // NOTE: add extra fields below here
  // list of senders blocked waiting for the task to receive
  struct MailQueue wait_for_receive;
  struct MailQueue wait_for_reply;
  struct Recvbuffer receive_buffer;
  struct MailQueueNode tempnode;
  struct Message outgoing_msg;
};

void tasks_init();

struct TaskDescriptor *task_create(struct TaskDescriptor *parent, int priority, void (*function)());
struct TaskDescriptor *task_get_current_task();
struct TaskDescriptor *task_get_by_tid();
void task_yield_current_task();
void task_schedule(struct TaskDescriptor *task);
void task_exit_current_task();
