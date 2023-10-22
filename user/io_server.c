#include "io_server.h"

#include <stdbool.h>
#include <stddef.h>

#include "../circular_buffer.h"
#include "../irq.h"
#include "../syscall.h"
#include "../uart.h"
#include "name_server.h"
#include "tid_queue.h"

static const int TASK_PRIORITY = 20;

void io_tx_task();
void io_rx_task();

void io_server_task() {
  printf("io_server_task: initializing child tasks\r\n");
  int console_rx_task = Create(TASK_PRIORITY, io_rx_task);
  enum Event console_rx_event = EVENT_UART_CONSOLE_RX;
  Send(console_rx_task, (const char *) &console_rx_event, sizeof(console_rx_event), NULL, 0);
  printf("io_server_task: sent info to tid %d\r\n", console_rx_task);
  printf("extra debug for fun\r\n");

  // int console_tx_task = Create(TASK_PRIORITY, io_tx_task);
  // enum Event console_tx_event = EVENT_UART_CONSOLE_TX;
  // Send(console_tx_task, (const char *) &console_tx_event, sizeof(console_tx_event), NULL, 0);

  // int marklin_rx_task = Create(TASK_PRIORITY, io_rx_task);
  // enum Event marklin_rx_event = EVENT_UART_MARKLIN_RX;
  // Send(marklin_rx_task, (const char *) &marklin_rx_event, sizeof(marklin_rx_event), NULL, 0);

  // int marklin_tx_task = Create(TASK_PRIORITY, io_tx_task);
  // enum Event marklin_tx_event = EVENT_UART_MARKLIN_TX;
  // Send(marklin_tx_task, (const char *) &marklin_tx_event, sizeof(marklin_tx_event), NULL, 0);

  Exit();
}

struct IOTxPutcRequest {
  unsigned char data;
};

struct IOTxPutlRequest {
  unsigned char *data;
  int datalen;
};

enum IOTxRequestType { TX_REQ_NOTIFY, TX_REQ_PUTC, TX_REQ_PUTL };

struct IOTxRequest {
  enum IOTxRequestType type;

  union {
    struct IOTxPutcRequest putc_req;
    struct IOTxPutlRequest putl_req;
  };
};

void io_tx_notify_task() {
  int tx_task;
  enum Event event;

  printf("io_tx_notify_task: waiting for parent to send info\r\n");
  Receive(&tx_task, (char *) &event, sizeof(event));
  Reply(tx_task, NULL, 0);

  printf("io_tx_notify_task: received event %d from tid %d\r\n", event, tx_task);

  while (true) {
    AwaitEvent(event);
    struct IOTxRequest req = {.type = TX_REQ_NOTIFY};
    Send(tx_task, (const char *) &req, sizeof(req), NULL, 0);
  }
}

void io_tx_task() {
  int parent_tid;
  enum Event event;

  printf("io_tx_task: waiting for parent to send info\r\n");
  Receive(&parent_tid, (char *) &event, sizeof(event));
  task_print();
  Reply(parent_tid, NULL, 0);
  printf("io_tx_task: received event %d from tid %d\r\n", event, parent_tid);
  task_print();

  int line;
  if (event == EVENT_UART_CONSOLE_TX) {
    line = UART_CONSOLE;
    RegisterAs("console_io_tx");
  } else {
    line = UART_MARKLIN;
    RegisterAs("marklin_io_tx");
  }

  struct CircularBuffer tx_buffer;
  circular_buffer_init(&tx_buffer);

  printf("creating notifier\r\n");
  // create notifier task
  int notifier_tid = Create(TASK_PRIORITY, io_tx_notify_task);
  Send(notifier_tid, (const char *) &event, sizeof(event), NULL, 0);
  printf("created notifier\r\n");

  int tid;
  struct IOTxRequest req;

  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case TX_REQ_NOTIFY:
        // unblock notify task
        Reply(tid, NULL, 0);

        // tx fifo buffer not empty
        // fill fifo buffer until full or tx_buffer empty
        while (!uart_tx_fifo_full(line) && !circular_buffer_empty(&tx_buffer)) {
          uart_putc(line, circular_buffer_read(&tx_buffer));
        }

        if (circular_buffer_empty(&tx_buffer)) {
          // disable interrupts since we have no more data to send
          uart_disable_tx_irq(line);
        }

        break;
      case TX_REQ_PUTC:
        circular_buffer_write(&tx_buffer, req.putc_req.data);
        uart_enable_tx_irq(line);

        Reply(tid, NULL, 0);
        break;
      case TX_REQ_PUTL:
        for (int i = 0; i < req.putl_req.datalen; ++i) {
          circular_buffer_write(&tx_buffer, req.putl_req.data[i]);
        }

        uart_enable_tx_irq(line);
        Reply(tid, NULL, 0);
    }
  }
}

enum IORxRequestType { RX_REQ_NOTIFY, RX_REQ_GETC };

void io_rx_notify_task() {
  int rx_task;
  enum Event event;

  printf("io_rx_notify_task: waiting for parent to send info\r\n");
  task_print();
  Receive(&rx_task, (char *) &event, sizeof(event));
  Reply(rx_task, NULL, 0);
  printf("io_rx_notify_task: received event %d from tid %d\r\n", event, rx_task);
  task_print();

  while (true) {
    printf("prior to await event\r\n");
    task_print();
    AwaitEvent(event);

    // notify rx task
    Send(rx_task, RX_REQ_NOTIFY, sizeof(RX_REQ_NOTIFY), NULL, 0);
  }
}

void io_rx_task() {
  int parent_tid;
  enum Event event;

  printf("io_rx_task: waiting for parent to send info\r\n");
  Receive(&parent_tid, (char *) &event, sizeof(event));
  printf("size of event %d\r\n", sizeof(event));
  printf("io_rx_task: before reply\r\n");
  
  task_print();
  printf("io_rx_task: received event %d from tid %d\r\n", event, parent_tid);
  Reply(parent_tid, NULL, 0);
  printf("io_rx_task: after reply\r\n");
  task_print();

  // int line;
  // if (event == EVENT_UART_CONSOLE_RX) {
  //   line = UART_CONSOLE;
  //   RegisterAs("console_io_rx");
  // } else {
  //   line = UART_MARKLIN;
  //   RegisterAs("marklin_io_rx");
  // }

  // struct CircularBuffer rx_buffer;
  // circular_buffer_init(&rx_buffer);

  // struct TIDQueue rx_queue;
  // tid_queue_init(&rx_queue);

  // printf("creating notifier\r\n");
  // // create notifier task
  // int notifier_tid = Create(TASK_PRIORITY, io_rx_notify_task);
  // Send(notifier_tid, (const char *) &event, sizeof(event), NULL, 0);
  // printf("created notifier\r\n");
  // task_print();

  // int tid;
  // enum IORxRequestType req_type;

  // while (true) {
  //   printf("receive call\r\n");
  //   Receive(&tid, (char *) &req_type, sizeof(req_type));
  //   printf("after receive\r\n");

  //   switch (req_type) {
  //     case RX_REQ_NOTIFY:
  //       // unblock notify task
  //       Reply(tid, NULL, 0);

  //       // read from DR, put in rx_buffer
  //       // transfer to rx_buffer from fifo buffer so interrupts stop firing
  //       while (uart_hasc(line)) {
  //         char c = uart_getc(line);
  //         circular_buffer_write(&rx_buffer, c);
  //         printf("received: %c\r\n", c);
  //       }

  //       // unblock tasks that are waiting for data
  //       while (!circular_buffer_empty(&rx_buffer) && !tid_queue_empty(&rx_queue)) {
  //         char ch = circular_buffer_read(&rx_buffer);
  //         Reply(tid_queue_poll(&rx_queue), &ch, sizeof(char));
  //       }

  //       break;
  //     case RX_REQ_GETC:
  //       if (!circular_buffer_empty(&rx_buffer)) {
  //         char ch = circular_buffer_read(&rx_buffer);
  //         Reply(tid, &ch, sizeof(char));
  //       } else {
  //         // block task and put it in a queue for when data is available
  //         tid_queue_add(&rx_queue, tid);
  //       }
  //   }
  // }
  Exit();
}

int Getc(int tid) {
  enum IORxRequestType req_type = RX_REQ_GETC;
  return Send(tid, (const char *) &req_type, sizeof(req_type), NULL, 0);
}

int Putc(int tid, unsigned char ch) {
  struct IOTxRequest req = {.type = TX_REQ_PUTC, .putc_req = {.data = ch}};
  return Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

int Putl(int tid, unsigned char *data, unsigned int len) {
  struct IOTxRequest req = {.type = TX_REQ_PUTL, .putl_req = {.data = data, .datalen = len}};
  return Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
