#include "replay_task.h"

#include <stdbool.h>

#include "../syscall.h"
#include "../uart.h"
#include "io_server.h"
#include "name_server.h"

void replay_task() {
  int console_rx = WhoIs("console_io_rx");
  int console_tx = WhoIs("console_io_tx");

  while (true) {
    char ch = Getc(console_rx);

    uart_putc(UART_CONSOLE, ch);
    Putc(console_tx, ch);
  }

  Exit();
}