#include "timer.h"

#include <stdint.h>
#include <stdio.h>

#include "irq.h"

#define TIMER_BASE 0xfe003000

static volatile uint32_t *const TIMER_CLO = (uint32_t *) ((char *) TIMER_BASE + 0x04);
static volatile uint32_t *const TIMER_CHI = (uint32_t *) ((char *) TIMER_BASE + 0x08);

static volatile uint32_t *const TIMER_C1 = (uint32_t *) ((char *) TIMER_BASE + 0x10);
static volatile uint32_t *const TIMER_C3 = (uint32_t *) ((char *) TIMER_BASE + 0x18);

void timer_init() {
  irq_enable(IRQ_TIMER_C1);
  // init delay
  timer_schedule_irq_c1(200000);
}

void timer_schedule_irq_c1(uint32_t delay) {
  *TIMER_C1 = *TIMER_CLO + delay;
  printf("timer schedule: %u\r\n", *TIMER_C1);
  printf("timer time: %lu\r\n", timer_get_time());
}

uint64_t timer_get_time() {
  // combine first 32 bits and last 32 bits of counter
  return ((uint64_t) *TIMER_CHI << 32) | *TIMER_CLO;
}
