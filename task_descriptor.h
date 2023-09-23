#include<memory.h>
typedef enum  {
  ACTIVE,
  READY,
  EXITED,
  SEND_BLOCKED,
  RECEIVE_BLOCKED,
  REPLY_BLOCKED,
  EVENT_BLOCKED,
  FREE
}TaskStatus;


typedef struct task_descriptor {
  int tid;

  TaskDescriptor *parent;
  TaskStatus status;
  int priority;
  uint32_t sp;

  struct task_descriptor *next;
  
  /**
   * missing:
   * the task’s current stack pointer.
   * a pointer to the TD of the next task in the task’s ready queue,
   * a point to the TD of the next task on the task’s send queue,
  */
}TaskDescriptor;
