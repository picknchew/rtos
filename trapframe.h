#define NUM_REGISTERS 30

struct Trapframe {
    uint64_t registers[NUM_REGISTERS];
    // SP_EL0
    uint64_t sp;
    uint64_t pc;
    // SPSR_EL1
    uint64_t pstate;
}
