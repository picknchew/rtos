#include "rpi.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "uart.h"
#include "util.h"

void uart_putl(size_t line, const char* buf, size_t blen) {
  uint32_t i;
  for (i = 0; i < blen; i++) {
    uart_putc(line, *(buf + i));
  }
}

// printf-style printing, with limited format support
void uart_format_print(size_t line, char* fmt, va_list va) {
  char bf[12];
  char ch;

  while ((ch = *(fmt++))) {
    if (ch != '%') {
      uart_putc(line, ch);
    } else {
      ch = *(fmt++);
      switch (ch) {
        case 'c':
          uart_putc(line, va_arg(va, unsigned int));
          break;
        case 'u':
          ui2a(va_arg(va, unsigned int), 10, bf);
          uart_puts(line, bf);
          break;
        case 'd':
          i2a(va_arg(va, int64_t), bf);
          uart_puts(line, bf);
          break;
        case 'x':
          ui2a(va_arg(va, unsigned int), 16, bf);
          uart_puts(line, bf);
          break;
        case 's':
          uart_puts(line, va_arg(va, char*));
          break;
        case '%':
          uart_putc(line, ch);
          break;
        case '\0':
          return;
      }
    }
  }
}

void uart_printf(size_t line, char* fmt, ...) {
  va_list va;

  va_start(va, fmt);
  uart_format_print(line, fmt, va);
  va_end(va);
}

void printf(char* fmt, ...) {
  va_list va;

  va_start(va, fmt);
  // line 1 for console
  uart_format_print(1, fmt, va);
  va_end(va);
}

void puts(char* str) {
  uart_puts(1, str);
}
