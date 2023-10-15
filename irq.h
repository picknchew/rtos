#include <stdint.h>

enum InterruptSource { IRQ_TIMER_C1 = 97, IRQ_TIMER_C3 = 99 };

void irq_enable(unsigned int irq_id);
void handle_irq();
