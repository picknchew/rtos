#include "exception.h"
#include "rpi.h"
#include "syscall.h"
#include "task.h"
#include "task_queue.h"

#define SVC(code) asm volatile ("svc %0" : : "I" (code) )

static const int CONSOLE = 1;
static const uint64_t INIT_EXCEPTION_INFO = 0x5400FFFF;

const char *train = "      oooOOOOOOOOOOO                                                       \r\n"
                    "     o   ____          :::::::::::::::::: :::::::::::::::::: __|-----|__   \r\n"
                    "     Y_,_|[]| --++++++ |[][][][][][][][]| |[][][][][][][][]| |  [] []  |   \r\n"
                    "    {|_|_|__|;|______|;|________________|;|________________|;|_________|;  \r\n"
                    "     /oo--OO   oo  oo   oo oo      oo oo   oo oo      oo oo   oo     oo    \r\n"
                    "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\r\n";

// return back to user mode
extern void kern_exit();

int kmain() {
  uart_config_and_enable(CONSOLE, 115200, false);
  uart_init();
  uart_puts(CONSOLE, train);
  uart_puts(CONSOLE, "Booting...\r\n");

  tasks_init();
  task_queues_init();

  // fake exception to start first task
  handle_exception(INIT_EXCEPTION_INFO);

  return 0;
}
