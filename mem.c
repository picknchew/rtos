#include "mem.h"

#include <stdint.h> 
#include <stddef.h>

static struct MemBlock *next_free_block;
static struct MemBlock blocks[MAX_MEM_BLOCKS];

// memory initialization
void mem_init(){
    uint32_t currentAddr = MEM_STACK_TOP;
    struct MemBlock *last = NULL;

    for (int i = 0; i < MAX_MEM_BLOCKS; i++) {
        currentAddr += BLOCK_SIZE;
        blocks[i].addr = currentAddr;
        blocks[i].next = last;
        last = &blocks[i];
    }
    
    next_free_block = last;
}
// allocate memory for each task
uint32_t mem_get_sp() {
    if (!next_free_block) {
        return 0;
        // no memory can be allocated
    }

    struct MemBlock *block = next_free_block;
    next_free_block = next_free_block->next;
    block->next = NULL;

    return (*block).addr;
}
