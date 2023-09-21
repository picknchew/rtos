enum TaskStatus {
  ACTIVE,
  READY,
  EXITED,
  SEND_BLOCKED,
  RECEIVE_BLOCKED,
  REPLY_BLOCKED,
  EVENT_BLOCKED
};

struct TaskDescriptor {
  int tid;

  TaskDescriptor *parent;
  TaskStatus status;
  int priority;
  
  /**
   * missing:
   * the task’s current stack pointer.
   * a pointer to the TD of the next task in the task’s ready queue,
   * a point to the TD of the next task on the task’s send queue,
  */
};
