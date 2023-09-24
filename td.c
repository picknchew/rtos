#include "task.h"
#include <stdlib.h>
#include <stdint.h> 
#define TASKS_SIZE 10
#define MEM_STACK_TOP 0x00230000
#define BLOCK_SIZE 0x10000
static struct TaskDescriptor tasks[TASKS_SIZE];
static struct TaskDescriptor *tasksp;
static struct TaskDescriptor *currentTask;
static struct MemBlock *last = NULL;

// commented out to compile
// void initTasks(){
//     int i;
//     uint32_t currentAddr = MEM_STACK_TOP;
//     for(i=0;i<TASKS_SIZE;i++){
//         tasks[i].status = FREE;
//         tasks[i].tid = i;
//         // linked list for next available task
//         if (i<TASKS_SIZE-1) tasks[i].next = &tasks[i+1];
//         else tasks[i].next = NULL;
//         tasks[i].sp = currentAddr+BLOCK_SIZE;
//         currentAddr = tasks[i].sp;
//     }
//     tasksp = &tasks[0];// pointer to next available taskdescriptor
// }

// struct TaskDescriptor *createTaskDescriptor(uint32_t priority) {
//     struct TaskDescriptor *t = NULL;
//     if (tasksp != NULL) {
//         t = tasksp;
//         tasksp = t->next; //?
//         t->parent = (currentTask == NULL)? -1 : currentTask->tid;
//         t->priority = priority;

//     }

// }