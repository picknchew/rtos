#pragma once

#include <stdint.h>

void timer_init();
uint64_t timer_get_time();
void timer_tick();
