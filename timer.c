#include "timer.h"

#include <stdint.h>
#include <stdio.h>

#include "irq.h"

#define TIMER_BASE 0xfe003000

static volatile uint32_t *const TIMER_CS = (uint32_t *) TIMER_BASE;

static volatile uint32_t *const TIMER_CLO = (uint32_t *) ((char *) TIMER_BASE + 0x04);
static volatile uint32_t *const TIMER_CHI = (uint32_t *) ((char *) TIMER_BASE + 0x08);

static volatile uint32_t *const TIMER_C1 = (uint32_t *) ((char *) TIMER_BASE + 0x10);
static volatile uint32_t *const TIMER_C3 = (uint32_t *) ((char *) TIMER_BASE + 0x18);

void timer_init() {
  irq_enable(IRQ_TIMER_C1);
  // init delay
  timer_schedule_irq_c1(200000);
}

static void clear_cs(int comparator) {
  // clear interrupt for c1
  *TIMER_CS = 1 << comparator;
}

void timer_schedule_irq_c1(uint32_t delay) {
  clear_cs(1);
  *TIMER_C1 = *TIMER_CLO + delay;
}

uint64_t timer_get_time() {
  // combine first 32 bits and last 32 bits of counter
  return ((uint64_t) *TIMER_CHI << 32) | *TIMER_CLO;
}
