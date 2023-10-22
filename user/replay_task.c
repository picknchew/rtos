#include "replay_task.h"

#include <stdbool.h>

#include "io_server.h"
#include "name_server.h"

int replay_task() {
  int console_rx = WhoIs("console_io_rx");
  int console_tx = WhoIs("console_io_tx");

  while (true) {
    char ch = Getc(console_rx);

    Putc(console_tx, ch);
  }
}
