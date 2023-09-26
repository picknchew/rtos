#include "exception.h"
#include "rpi.h"
#include "syscall.h"
#include "task.h"
#include "task_queue.h"

#define SVC(code) asm volatile ("svc %0" : : "I" (code) )

static const int CONSOLE = 1;

const char *train = "      oooOOOOOOOOOOO                                                       \r\n"
                    "     o   ____          :::::::::::::::::: :::::::::::::::::: __|-----|__   \r\n"
                    "     Y_,_|[]| --++++++ |[][][][][][][][]| |[][][][][][][][]| |  [] []  |   \r\n"
                    "    {|_|_|__|;|______|;|________________|;|________________|;|_________|;  \r\n"
                    "     /oo--OO   oo  oo   oo oo      oo oo   oo oo      oo oo   oo     oo    \r\n"
                    "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\r\n";

int kmain() {
  uart_config_and_enable(CONSOLE, 115200, false);
  uart_init();
  uart_puts(CONSOLE, train);
  uart_puts(CONSOLE, "Booting...\r\n");

  tasks_init();
  task_queues_init();

  // SYSCALL_EXIT = 1442840581 = 0x5600005
  // call to start first task
  handle_exception(0x56000005);

  return 0;
}
