#ifndef _rpi_h_
#define _rpi_h_ 1

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

unsigned char uart_getc(size_t line);
void uart_putl(size_t line, const char *buf, size_t blen);
void uart_printf(size_t line, char *fmt, ...);
void uart_format_print(size_t line, char *fmt, va_list va);
void printf(char *fmt, ...);
void puts(char *str);

#endif /* rpi.h */
