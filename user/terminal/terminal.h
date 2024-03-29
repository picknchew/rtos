#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COMMAND_BUFFER_SIZE 64

struct Terminal {
  char command_buffer[COMMAND_BUFFER_SIZE];
  size_t command_len;

  int screen_tid;
};

void terminal_init(struct Terminal *terminal, int terminal_screen);
bool terminal_handle_keypress(
    struct Terminal *terminal,
    int train_tid,
    int train_calib_tid,
    int train_router_tid,
    char c
);
bool terminal_execute_command(
    struct Terminal *terminal,
    int train_tid,
    int train_calib_tid,
    int train_router_tid,
    char *command
);
void terminal_clear_command_buffer(struct Terminal *terminal);
