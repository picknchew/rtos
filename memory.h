#include <stdint.h> 
// need to figure out the address number
#define MEM_STACK_TOP 
#define blocks_number 10
#define BLOCK_SIZE

// for each task, need a memory block
typedef struct memblock{
    uint32_t addr;
    struct memblock *next;
}taskMemblock;

void initMem();
uint32_t getsp();