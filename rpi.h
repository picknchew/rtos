#ifndef _rpi_h_
#define _rpi_h_ 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool uart_hasc(size_t line);
void uart_putc(size_t line, unsigned char c);
unsigned char uart_getc(size_t line);
void uart_putl(size_t line, const char *buf, size_t blen);
void uart_puts(size_t line, const char *buf);
void uart_printf(size_t line, char *fmt, ...);
void uart_config_and_enable(size_t line, uint32_t baudrate, bool two_stop_bits);
void uart_init();

#endif /* rpi.h */
