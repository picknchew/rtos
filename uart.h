#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "irq.h"

extern const size_t UART_MARKLIN;
extern const size_t UART_CONSOLE;

void uart_init();
void uart_config_and_enable(
    size_t line,
    uint32_t baudrate,
    bool two_stop_bits,
    bool fifo_buffer,
    bool cts_irq);
enum Event uart_handle_irq();
void uart_enable_tx_irq(size_t line);
void uart_disable_tx_irq(size_t line);
bool uart_tx_asserted(size_t line);
bool uart_tx_fifo_full(size_t line);
bool uart_hasc(size_t line);
unsigned char uart_getc(size_t line);
void uart_putc(size_t line, unsigned char ch);
void uart_puts(size_t line, const char* buf);
bool uart_cts(size_t line);
