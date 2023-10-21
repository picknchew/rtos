#pragma once

#include <stdint.h>

// must equal to the number of events
#define EVENT_MAX 2

enum Event {
  EVENT_UNKNOWN,
  EVENT_TIMER,
  EVENT_UART_CONSOLE_RX,
  EVENT_UART_CONSOLE_TX,
  EVENT_UART_MARKLIN_RX,
  EVENT_UART_MARKLIN_TX
};

enum InterruptSource { IRQ_TIMER_C1 = 97, IRQ_TIMER_C3 = 99, IRQ_UART = 153, IRQ_SPURIOUS = 1023 };

void irq_init();
void irq_enable(enum InterruptSource irq_id);
void irq_await_event(enum Event event);
void handle_irq();
