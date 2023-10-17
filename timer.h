#pragma once

#include <stdint.h>

extern const uint32_t TIMER_TICK_DURATION;

void timer_init();
uint64_t timer_get_time();
void timer_tick();
