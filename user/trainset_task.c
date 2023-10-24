#include "trainset_task.h"


#include "../syscall.h"
#include "../train/terminal.h"
#include "../train/trainset.h"
#include "../timer.h"
#include "name_server.h"
#include "io_server.h"
#include <stdio.h>
#include "../uart.h"


void trainset_task() {
  struct Terminal* terminal;
  struct Trainset* trainset;
  int marklin_tx_server = WhoIs("marklin_io_tx");
  int console_tx_server = WhoIs("console_io_tx");
  int console_rx_server = WhoIs("console_io_rx");
  int marklin_rx_server = WhoIs("marklin_io_rx");
  
  trainset_init(trainset,marklin_rx_server,marklin_tx_server);
  terminal_init(terminal,trainset,console_rx_server,console_tx_server);

  

  
  // parameter_init_terminal(console_tx_server,console_rx_server);
  for(;;){
    terminal_tick(terminal,timer_get_time(),100);
    trainset_tick(trainset,timer_get_time(),terminal);
  }
  Exit();
  // char speed_command[] = {6,54}; //speed,number
  // char stop_command[] = {0,54};
  // char command;
  // while(1){
  //   printf("while loop \r\n");
  //   command = Getc(console_rx_server);
  //   uart_putc(UART_CONSOLE, command);
  //   if (command == 'q'){
  //     Exit();
  //   }else if (command == 't'){
  //     // tr command
  //     printf("1");
  //     Putc(marklin_tx_server,command);
  //     // Putl(marklin_tx_server,(unsigned char *)speed_command,2);
  //     printf("2");
  //   }else if (command == 'r'){
  //     // reverse command
  //     Putl(marklin_tx_server,(unsigned char *)stop_command,2);
  //   }else if (command == 's'){
  //     // sensor command
  //     Putc(marklin_tx_server,133);
  //     for(int i = 0;i<=9;i++){
  //       char a = Getc(marklin_rx_server);
  //       uart_putc(UART_CONSOLE, a);
  //     }
  //   }
  // }
}



