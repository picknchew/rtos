#pragma once
#include <stddef.h>
#include <stdint.h>

#define NUM_REGISTERS 31

enum TaskStatus {
  ACTIVE,
  READY,
  EXITED,
  SEND_BLOCKED,
  RECEIVE_BLOCKED,
  REPLY_BLOCKED,
  EVENT_BLOCKED
};

struct TaskContext {
    uint64_t registers[NUM_REGISTERS];
    // SP_EL0
    uint64_t sp;
    // ELR_0 (PC)
    uint64_t lr;
    // SPSR_EL1
    uint64_t pstate;
};

struct TaskDescriptor {
  unsigned int tid;
  unsigned int priority;

  struct TaskDescriptor *parent; // offset: 8
  enum TaskStatus status;

  struct TaskContext context;

  // block of memory allocated for this task
  // when a task stops running
  uint64_t *stack;
  
  // NOTE: add extra fields below here

  /**
   * missing:
   * a pointer to the TD of the next task in the task’s ready queue,
   * a pointer to the TD of the next task on the task’s send queue,
  */
};
