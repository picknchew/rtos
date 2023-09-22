#include "rpi.h"

#define SVC(code) asm volatile ("svc %0" : : "I" (code) )

//https://github.com/s-matyukevich/raspberry-pi-os/blob/master/docs/lesson03/rpi-os.md
//https://github.com/isometimes/rpi4-osdev/tree/master/part13-interrupts

const int CONSOLE = 1;

void exception_handler() {
  uart_puts(CONSOLE, "exception handler\r\n");

  for (;;) {}

  uart_puts(CONSOLE, "after loop\r\n");
}

int kmain() {
  uart_config_and_enable(CONSOLE, 115200, false);
  uart_init();
  uart_puts(CONSOLE, "kmain called\r\n");

  SVC(0);

  return 0;
}
