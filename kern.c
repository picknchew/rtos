#include "rpi.h"

int kmain() {
  const int CONSOLE = 0;

  uart_config_and_enable(CONSOLE, 115200, false);
  uart_init();
  uart_puts(CONSOLE, "Hello, world!\r\n");
  return 0;
}
