#include "terminal_screen.h"

#include "syscall.h"
#include "user/server/io_server.h"
#include "user/server/name_server.h"
#include "user/train/trainset.h"
#include "util.h"

static const char SEQ_CLEAR_SCREEN[] = "\033[2J";
static const char SEQ_CURSOR_DELETE_LINE[] = "\033[K";

static const char SEQ_CURSOR_HIDE[] = "\033[?25l";
static const char SEQ_CURSOR_SHOW[] = "\033[?25h";

static const char SEQ_CURSOR_MOVE_TOP_LEFT[] = "\033[H";

const char TEXT_RESET[] = "\033[0m";

void terminal_putc(struct TerminalScreen *screen, char ch) {
  Putc(screen->console_tx, ch);
}

void terminal_putl(struct TerminalScreen *screen, const char *buf, size_t blen) {
  Putl(screen->console_tx, (const unsigned char *) buf, blen);
}

int terminal_puts(struct TerminalScreen *screen, const char *str) {
  int len = strlen(str);
  terminal_putl(screen, str, len);
  return len;
}

void terminal_format_print(struct TerminalScreen *screen, char *fmt, va_list va) {
  char bf[12];
  char ch;

  while ((ch = *(fmt++))) {
    if (ch != '%') {
      terminal_putc(screen, ch);
    } else {
      ch = *(fmt++);

      int padding = 0;
      while (ch >= '0' && ch <= '9') {
        padding = padding * 10 + (ch - '0');
        ch = *(fmt++);
      }

      int printed = 0;

      switch (ch) {
        case 'u':
          ui2a(va_arg(va, unsigned int), 10, bf);
          printed = terminal_puts(screen, bf);
          break;
        case 'd':
          i2a(va_arg(va, int), bf);
          printed = terminal_puts(screen, bf);
          break;
        case 'x':
          ui2a(va_arg(va, unsigned int), 16, bf);
          printed = terminal_puts(screen, bf);
          break;
        case 'c':
          terminal_putc(screen, va_arg(va, int));
          printed = 1;
          break;
        case 's':
          printed = terminal_puts(screen, va_arg(va, char *));
          break;
        case '%':
          terminal_putc(screen, ch);
          printed = 1;
          break;
        case '\0':
          return;
      }
      for (int i = 0; i < padding - printed; ++i) {
        terminal_putc(screen, ' ');
      }
    }
  }
}

void terminal_printf(struct TerminalScreen *screen, char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  terminal_format_print(screen, fmt, va);
  va_end(va);
}

void terminal_move_cursor(struct TerminalScreen *screen, int r, int c) {
  terminal_printf(screen, "\033[%d;%dH", r, c);
}

void terminal_save_cursor(struct TerminalScreen *screen) {
  terminal_puts(screen, "\0337");
}

void terminal_restore_cursor(struct TerminalScreen *screen) {
  terminal_puts(screen, "\0338");
}

void terminal_cursor_reset_text(struct TerminalScreen *screen) {
  terminal_puts(screen, TEXT_RESET);
}

void terminal_print_title(struct TerminalScreen *screen, char *title) {
  terminal_printf(screen, "\033[36m%s", title);
  terminal_puts(screen, TEXT_RESET);
}

void terminal_clear_screen(struct TerminalScreen *screen) {
  terminal_puts(screen, SEQ_CLEAR_SCREEN);
}

void terminal_cursor_move_top_left(struct TerminalScreen *screen) {
  terminal_puts(screen, SEQ_CURSOR_MOVE_TOP_LEFT);
}

void terminal_cursor_delete_line(struct TerminalScreen *screen) {
  terminal_puts(screen, SEQ_CURSOR_DELETE_LINE);
}

void terminal_hide_cursor(struct TerminalScreen *screen) {
  terminal_puts(screen, SEQ_CURSOR_HIDE);
}

void terminal_show_cursor(struct TerminalScreen *screen) {
  terminal_puts(screen, SEQ_CURSOR_SHOW);
}

void terminal_update_status(struct TerminalScreen *screen, char *fmt, ...) {
  va_list va;

  va_start(va, fmt);
  terminal_update_status_va(screen, fmt, va);
  va_end(va);
}
