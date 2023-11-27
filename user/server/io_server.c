#include "io_server.h"

#include <stdbool.h>
#include <stddef.h>

#include "circular_buffer.h"
#include "irq.h"
#include "name_server.h"
#include "syscall.h"
#include "uart.h"
#include "user/tid_queue.h"

const int IO_TASK_PRIORITY = 20;

void io_tx_task();
void io_rx_task();
void io_marklin_tx_task();

void io_server_task() {
  int console_rx_task = Create(IO_TASK_PRIORITY, io_rx_task);
  enum Event console_rx_event = EVENT_UART_CONSOLE_RX;
  Send(console_rx_task, (const char *) &console_rx_event, sizeof(console_rx_event), NULL, 0);

  int console_tx_task = Create(IO_TASK_PRIORITY, io_tx_task);
  enum Event console_tx_event = EVENT_UART_CONSOLE_TX;
  Send(console_tx_task, (const char *) &console_tx_event, sizeof(console_tx_event), NULL, 0);

  int marklin_rx_task = Create(IO_TASK_PRIORITY, io_rx_task);
  enum Event marklin_rx_event = EVENT_UART_MARKLIN_RX;
  Send(marklin_rx_task, (const char *) &marklin_rx_event, sizeof(marklin_rx_event), NULL, 0);

  Create(IO_TASK_PRIORITY, io_marklin_tx_task);

  Exit();
}

struct IOTxPutcRequest {
  unsigned char data;
};

struct IOTxPutlRequest {
  const unsigned char *data;
  int datalen;
};

enum IOTxRequestType { TX_REQ_NOTIFY_TX, TX_REQ_PUTC, TX_REQ_PUTL, TX_REQ_NOTIFY_CTS, TX_REQ_READ };

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

  Receive(&tx_task, (char *) &event, sizeof(event));
  Reply(tx_task, NULL, 0);

  struct IOTxRequest req = {.type = TX_REQ_NOTIFY_TX};
  while (true) {
    AwaitEvent(event);
    Send(tx_task, (const char *) &req, sizeof(req), NULL, 0);
  }
}

void io_marklin_tx_notify_cts_task() {
  int marklin_tx_io_task = MyParentTid();

  struct IOTxRequest req = {.type = TX_REQ_NOTIFY_CTS};
  while (true) {
    AwaitEvent(EVENT_UART_MARKLIN_CTS);
    Send(marklin_tx_io_task, (const char *) &req, sizeof(req), NULL, 0);
  }
}

enum MarklinState { MARKLIN_READY, MARKLIN_CMD_SENT, MARKLIN_BUSY };

void io_marklin_tx_task() {
  int clock_server = WhoIs("clock_server");

  bool done_reading = true;

  RegisterAs("marklin_io_tx");

  int line = UART_MARKLIN;

  struct CircularBuffer tx_buffer;
  circular_buffer_init(&tx_buffer);

  // create notifier task
  Create(NOTIFIER_PRIORITY, io_marklin_tx_notify_cts_task);

  int tid;
  struct IOTxRequest req;

  enum MarklinState marklin_state = MARKLIN_READY;

  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case TX_REQ_NOTIFY_CTS:
        if (marklin_state == MARKLIN_CMD_SENT) {
          marklin_state = MARKLIN_BUSY;
        } else if (marklin_state == MARKLIN_BUSY) {
          marklin_state = MARKLIN_READY;
        }

        Reply(tid, NULL, 0);
        break;
      case TX_REQ_PUTC:
        circular_buffer_write(&tx_buffer, req.putc_req.data);

        Reply(tid, NULL, 0);

        break;
      case TX_REQ_PUTL:
        for (int i = 0; i < req.putl_req.datalen; ++i) {
          circular_buffer_write(&tx_buffer, req.putl_req.data[i]);
        }

        // must reply after, if we reply before, the request data can be corrupted
        // in the middle of our write to UART.
        Reply(tid, NULL, 0);
        break;
      case TX_REQ_READ:
        done_reading = true;
        Reply(tid, NULL, 0);
        break;
      default:
        // do not use tx
        break;
    }

    if (!circular_buffer_empty(&tx_buffer) && marklin_state == MARKLIN_READY && done_reading) {
      Delay(clock_server, 1);

      char ch = circular_buffer_read(&tx_buffer);
      uart_putc(line, ch);

      // read all sensors
      if (ch == 0x80 + 5) {
        done_reading = false;
      }

      marklin_state = MARKLIN_CMD_SENT;
    }
  }
}

void io_tx_task() {
  int parent_tid;
  enum Event event;

  Receive(&parent_tid, (char *) &event, sizeof(event));
  Reply(parent_tid, NULL, 0);

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

  // create notifier task
  int notifier_tid = Create(NOTIFIER_PRIORITY, io_tx_notify_task);
  Send(notifier_tid, (const char *) &event, sizeof(event), NULL, 0);

  int tid;
  struct IOTxRequest req;

  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case TX_REQ_NOTIFY_TX:
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
        if (!circular_buffer_empty(&tx_buffer) || uart_tx_fifo_full(line)) {
          circular_buffer_write(&tx_buffer, req.putc_req.data);
        } else {
          uart_putc(line, req.putc_req.data);
        }

        Reply(tid, NULL, 0);
        break;
      case TX_REQ_PUTL:
        uart_enable_tx_irq(line);

        for (int i = 0; i < req.putl_req.datalen; ++i) {
          circular_buffer_write(&tx_buffer, req.putl_req.data[i]);
        }

        // write as much as we can and wait for tx
        while (!uart_tx_fifo_full(line) && !circular_buffer_empty(&tx_buffer)) {
          uart_putc(line, circular_buffer_read(&tx_buffer));
        }

        Reply(tid, NULL, 0);
        break;
      default:
        // do not use cts
        break;
    }
  }
}

enum IORxRequestType { RX_REQ_NOTIFY, RX_REQ_GETC };

void io_rx_notify_task() {
  int rx_task;
  enum Event event;

  Receive(&rx_task, (char *) &event, sizeof(event));
  Reply(rx_task, NULL, 0);

  enum IORxRequestType req = RX_REQ_NOTIFY;
  while (true) {
    AwaitEvent(event);

    // notify rx task
    Send(rx_task, (char *) &req, sizeof(req), NULL, 0);
  }
}

void io_rx_task() {
  int parent_tid;
  enum Event event;

  Receive(&parent_tid, (char *) &event, sizeof(event));
  Reply(parent_tid, NULL, 0);

  size_t line;
  if (event == EVENT_UART_CONSOLE_RX) {
    line = UART_CONSOLE;
    RegisterAs("console_io_rx");
  } else {
    line = UART_MARKLIN;
    RegisterAs("marklin_io_rx");
  }

  struct CircularBuffer rx_buffer;
  circular_buffer_init(&rx_buffer);

  struct TIDQueue rx_queue;
  tid_queue_init(&rx_queue);

  // create notifier task
  int notifier_tid = Create(NOTIFIER_PRIORITY, io_rx_notify_task);
  Send(notifier_tid, (const char *) &event, sizeof(event), NULL, 0);

  int tid;
  enum IORxRequestType req_type;

  while (true) {
    Receive(&tid, (char *) &req_type, sizeof(req_type));

    switch (req_type) {
      case RX_REQ_NOTIFY:
        // unblock notify task
        Reply(tid, NULL, 0);

        // read from DR, put in rx_buffer
        // transfer to rx_buffer from fifo buffer so interrupts stop firing
        while (uart_hasc(line)) {
          char c = uart_getc(line);
          circular_buffer_write(&rx_buffer, c);
        }

        // unblock tasks that are waiting for data
        while (!circular_buffer_empty(&rx_buffer) && !tid_queue_empty(&rx_queue)) {
          char ch = circular_buffer_read(&rx_buffer);
          Reply(tid_queue_poll(&rx_queue), &ch, sizeof(char));
        }

        break;
      case RX_REQ_GETC:
        if (!circular_buffer_empty(&rx_buffer)) {
          char ch = circular_buffer_read(&rx_buffer);
          Reply(tid, &ch, sizeof(char));
        } else {
          // block task and put it in a queue for when data is available
          tid_queue_add(&rx_queue, tid);
        }
    }
  }
  Exit();
}

int Getc(int tid) {
  enum IORxRequestType req_type = RX_REQ_GETC;

  char ch;
  if (Send(tid, (const char *) &req_type, sizeof(req_type), &ch, sizeof(ch)) <= 0) {
    return -1;
  }
  return ch;
}

int Putc(int tid, unsigned char ch) {
  struct IOTxRequest req = {.type = TX_REQ_PUTC, .putc_req = {.data = ch}};
  return Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

int Putl(int tid, const unsigned char *data, unsigned int len) {
  struct IOTxRequest req = {.type = TX_REQ_PUTL, .putl_req = {.data = data, .datalen = len}};
  return Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}

int NotifyMarklinRead(int tid) {
  struct IOTxRequest req = {.type = TX_REQ_READ};
  return Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
