#include "task_queue.h"

// one TaskQueueNode for every task
// indexed by tid
// a task descriptor cannot be in two queues at the same time
static struct TaskQueueNode task_queue_nodes[TASKS_MAX] = { {0} };

void task_queues_init() {
  for (int i = 0; i < TASKS_MAX; ++i) {
    task_queue_nodes[i].val = task_get_by_tid(i);
    task_queue_nodes[i].next = NULL;
  }
}

static void task_queue_add(struct TaskQueue *task_queue, struct TaskDescriptor *task) {
  struct TaskQueueNode *node = &task_queue_nodes[task->tid];

  node->next = NULL;

  if (task_queue->tail) {
    task_queue->tail->next = node;
  } else {
    task_queue->head = node;
    task_queue->tail = node;
  }

  ++task_queue->size;
  uart_printf(1, "added task %d, new size of queue: %d\r\n", task->tid, task_queue->size);
}

void priority_task_queue_init(struct PriorityTaskQueue *queue) {
  // init queues
  for (int i = 0; i < MAX_PRIORITY; ++i) {
    struct TaskQueue *task_queue = &queue->queues[i];

    task_queue->head = NULL;
    task_queue->tail = NULL;
    task_queue->size = 0;
  }
}

static struct TaskQueueNode *task_queue_pop(struct TaskQueue *task_queue) {
  struct TaskQueueNode *popped = task_queue->head;

  task_queue->head = popped->next;
  --task_queue->size;
  
  if (task_queue->size <= 1) {
    task_queue->tail = task_queue->head;
  }

  return popped;
}

struct TaskDescriptor *priority_task_queue_pop(struct PriorityTaskQueue *queue) {
  // find queue with highest priority with task
  struct TaskQueue *task_queue = NULL;

  // starting from highest priority
  for (int i = MAX_PRIORITY - 1; i >= 0; --i) {
    if (queue->queues[i].size > 0) {
      task_queue = &queue->queues[i];
      break;
    }
  }

  if (!task_queue) {
    return NULL;
  }

  return task_queue_pop(task_queue)->val;
}

void priority_task_queue_push(struct PriorityTaskQueue *queue, struct TaskDescriptor *task) {
  task_queue_add(&queue->queues[task->priority], task);
}

struct TaskDescriptor *priority_task_queue_delete(struct PriorityTaskQueue *queue, struct TaskDescriptor *td) {
  struct TaskQueue *task_queue = NULL;
  for (int i = MAX_PRIORITY - 1; i >= 0; --i) {
    if (queue->queues[i].size > 0) {
      task_queue = &queue->queues[i];
      struct TaskQueueNode* cur = task_queue->head;
      struct TaskQueueNode* pre;
      while(cur->val!=td&&cur!=NULL) {
        pre = cur;
        cur = cur->next;
      }
      if (cur==NULL) continue;
      else {
        pre->next  = cur->next;
        cur->next = NULL;
        return td;
      } 
    }
  }return td;
}
