#include "exception.h"
#include "rpi.h"

#define SVC(code) asm volatile ("svc %0" : : "I" (code) )

static const int CONSOLE = 1;

const char *train = "      oooOOOOOOOOOOO                                                       \r\n"
                    "     o   ____          :::::::::::::::::: :::::::::::::::::: __|-----|__   \r\n"
                    "     Y_,_|[]| --++++++ |[][][][][][][][]| |[][][][][][][][]| |  [] []  |   \n\n"
                    "    {|_|_|__|;|______|;|________________|;|________________|;|_________|;  \r\n"
                    "     /oo--OO   oo  oo   oo oo      oo oo   oo oo      oo oo   oo     oo    \r\n"
                    "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\r\n";

// NOTE TO US: for the init task we need to create a PSTATE that is at EL0

int kmain() {
  uart_config_and_enable(CONSOLE, 115200, false);
  uart_init();
  uart_puts(CONSOLE, train);
  uart_puts(CONSOLE, "Booting...\r\n");

  SVC(EX_TEST);

  return 0;
}
