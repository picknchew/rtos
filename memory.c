#include<memory.h>
#include <stdint.h> 
#include <stddef.h>
static taskMemblock *mempointer;
static taskMemblock memblocks[blocks_number];

// memory initialization
void initMem(){
    uint32_t currentAddr = MEM_STACK_TOP;
    taskMemblock *last = NULL;
    int i;
    for(i=0;i<blocks_number;i++){
        currentAddr += BLOCK_SIZE;
        memblocks[i].addr = currentAddr;
        memblocks[i].next = last;
        last = &memblocks[i];
    }mempointer = last;
}
// allocate memory for each task
uint32_t getsp(){
    if (mempointer==NULL){
        return NULL;
        // no memmory can be allocated
    }
    taskMemblock *block = mempointer;
    mempointer = mempointer->next;
    block->next = NULL;
    return (*block).addr;
}
