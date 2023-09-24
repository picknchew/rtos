#include <stdint.h> 
// need to figure out the address number
#define MEM_STACK_TOP 0x10000
#define MAX_MEM_BLOCKS 10
#define BLOCK_SIZE 2048

// for each task, need a memory block
struct MemBlock {
    uint32_t addr;
    struct MemBlock *next;
};

void mem_init();
uint32_t get_sp();
