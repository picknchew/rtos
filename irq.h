#pragma once

#include <stdint.h>

#include "task.h"
#include "task_queue.h"

// must equal to the number of events
#define EVENT_MAX 10

// max priority
#define NOTIFIER_PRIORITY (MAX_PRIORITY - 1)

enum Event {
  EVENT_UNKNOWN = 0,  // start at 0 since we use these as indices for our event queue array
  EVENT_TIMER,
  EVENT_UART_CONSOLE_RX,
  EVENT_UART_CONSOLE_TX,
  EVENT_UART_CONSOLE_CTS,
  EVENT_UART_MARKLIN_RX,
  EVENT_UART_MARKLIN_TX,
  EVENT_UART_MARKLIN_CTS_ON,
  EVENT_UART_MARKLIN_CTS_OFF,
  EVENT_IGNORE
};

enum InterruptSource { IRQ_TIMER_C1 = 97, IRQ_TIMER_C3 = 99, IRQ_UART = 153, IRQ_SPURIOUS = 1023 };

void irq_init();
void irq_enable(enum InterruptSource irq_id);
void irq_await_event(enum Event event);
void handle_irq();
