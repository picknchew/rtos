#pragma once

#include <stdint.h>

// must equal to the number of events
#define EVENT_MAX 2

enum Event { EVENT_UNKNOWN = 0, EVENT_TIMER };
enum InterruptSource { IRQ_TIMER_C1 = 97, IRQ_TIMER_C3 = 99 };

void irq_init();
void irq_enable(enum InterruptSource irq_id);
void irq_await_event(enum Event event);
void handle_irq();
