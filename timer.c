#include "timer.h"

#include <stdint.h>

#define TIMER_BASE 0xFE003000

static volatile uint32_t *const TIMER_CLO = (uint32_t *) ((char *)TIMER_BASE + 0x04);
static volatile uint32_t *const TIMER_CHI = (uint32_t *) ((char *)TIMER_BASE + 0x08);

uint64_t timer_get_time() {
  // combine first 32 bits and last 32 bits of counter
  return ((uint64_t)*TIMER_CHI << 32) | *TIMER_CLO;
}
