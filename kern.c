#include "exception.h"
#include "irq.h"
#include "syscall.h"
#include "task.h"
#include "task_queue.h"
#include "timer.h"
#include "uart.h"
#include "user/init_task.h"

#define SVC(code) asm volatile("svc %0" : : "I"(code))

static const uint64_t SVC_EXCEPTION_INFO = 0x54000000;

const char *train =
    "      oooOOOOOOOOOOO                                                       \r\n"
    "     o   ____          :::::::::::::::::: :::::::::::::::::: __|-----|__   \r\n"
    "     Y_,_|[]| --++++++ |[][][][][][][][]| |[][][][][][][][]| |  [] []  |   \r\n"
    "    {|_|_|__|;|______|;|________________|;|________________|;|_________|;  \r\n"
    "     /oo--OO   oo  oo   oo oo      oo oo   oo oo      oo oo   oo     oo    \r\n"
    "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\r\n";

// return back to user mode
extern void kern_exit();

int kmain() {
  uart_config_and_enable(UART_CONSOLE, 115200, false, true);
  uart_init();
  uart_puts(UART_CONSOLE, train);
  uart_puts(UART_CONSOLE, "Booting...\r\n");

  irq_init();
  timer_init();

  tasks_init();
  task_queues_init();

  struct TaskDescriptor *task = task_create(NULL, 0, init_task);
  // schedule initial task
  task_schedule(task);

  // fake exception to start first task
  handle_exception(SVC_EXCEPTION_INFO | SYSCALL_INIT);

  return 0;
}
