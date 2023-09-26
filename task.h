#pragma once
#include <stddef.h>
#include <stdint.h>

#define TASKS_MAX 128
#define NUM_REGISTERS 31

#define STACK_SIZE 4096

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

struct TaskDescriptor {
  uint32_t tid;
  uint32_t priority;

  struct TaskContext context; // offset: 8

  struct TaskDescriptor *parent;
  enum TaskStatus status;

  // block of memory allocated for this task
  uint64_t stack[STACK_SIZE];
  
  // NOTE: add extra fields below here

  /**
   * missing:
   * a pointer to the TD of the next task in the task’s ready queue,
   * a pointer to the TD of the next task on the task’s send queue,
  */
};

void tasks_init();

struct TaskDescriptor *task_create(struct TaskDescriptor *parent, int priority, void (*function)());
struct TaskDescriptor *task_get_current_task();
struct TaskDescriptor *task_get_by_tid();
void task_yield_current_task();
void task_schedule(struct TaskDescriptor *task);
void task_exit_current_task();
