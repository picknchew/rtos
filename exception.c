#include "exception.h"

#include "rpi.h"
#include "task.h"

static const int SYSCALL_TYPE_MASK = 0xFFFF;

static const int EC_MASK = 0x3F;
// syscall exception class
static const int EC_SVC = 0x15;

static const int CONSOLE = 1;

void handle_exception(int exception_info) {
  uart_puts(CONSOLE, "handle_exception - start\r\n");

  int exception_class = exception_info >> 26 & EC_MASK;

  if (exception_class != EC_SVC) {
    uart_printf(CONSOLE, "not svc exception! %d\r\n", exception_class);
  }

  struct TaskDescriptor *current_task = task_get_current_task();
  enum ExceptionType ex_type = exception_info & SYSCALL_TYPE_MASK;

  switch (ex_type) {
    case EX_TEST:
      uart_puts(CONSOLE, "ex_test\r\n");
      break;
    default:
      break;
  }

  uart_puts(CONSOLE, "handle_exception - end\r\n");

  for (;;) {
    // spin
  }
}
