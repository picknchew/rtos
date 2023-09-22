#include "rpi.h"

#define VBAR_EL1

//https://github.com/s-matyukevich/raspberry-pi-os/blob/master/docs/lesson03/rpi-os.md
//https://github.com/isometimes/rpi4-osdev/tree/master/part13-interrupts

int kmain() {
  const int CONSOLE = 0;

  uart_config_and_enable(CONSOLE, 115200, false);
  uart_init();
  uart_puts(CONSOLE, "Hello, world!\r\n");
  return 0;
}
