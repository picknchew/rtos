#include "rpi.h"

#define VBAR_EL1

void init_vector_table() {

}

int kmain() {
  const int CONSOLE = 0;

  uart_config_and_enable(CONSOLE, 115200, false);
  uart_init();
  uart_puts(CONSOLE, "Hello, world!\r\n");
  return 0;
}
