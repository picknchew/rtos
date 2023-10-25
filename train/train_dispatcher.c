#include "train_dispatcher.h"

#include <stdbool.h>

#include "../circular_buffer.h"
#include "../irq.h"
#include "../syscall.h"
#include "../user/io_server.h"
#include "../user/name_server.h"

static const int DISPATCHER_PRIORITY = 19;

struct TrainCommand {
  const unsigned char *data;
  int len;
};

enum MarklinState { MARKLIN_READY, MARKLIN_CMD_SENT, MARKLIN_BUSY };

enum TrainDispatchRequestType { NOTIFY, TRAIN_DISPATCH };

struct TrainDispatchRequest {
  enum TrainDispatchRequestType type;
  struct TrainCommand cmd;
};

void train_dispatcher_notifier() {
  int dispatcher = MyParentTid();

  struct TrainDispatchRequest req = {.type = NOTIFY};
  while (true) {
    AwaitEvent(EVENT_UART_MARKLIN_CTS);
    Send(dispatcher, (const char *) &req, sizeof(req), NULL, 0);
  }
}

void train_dispatcher_task() {
  RegisterAs("train_dispatch");

  int marklin_tx = WhoIs("marklin_io_tx");

  Create(DISPATCHER_PRIORITY, train_dispatcher_notifier);

  enum MarklinState marklin_state = MARKLIN_READY;

  struct CircularBuffer cmd_buffer;
  circular_buffer_init(&cmd_buffer);

  int tid;
  struct TrainDispatchRequest req;
  while (true) {
    Receive(&tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case NOTIFY:
        Reply(tid, NULL, 0);
        if (marklin_state == MARKLIN_CMD_SENT) {
          marklin_state = MARKLIN_BUSY;
        } else if (marklin_state == MARKLIN_BUSY) {
          marklin_state = MARKLIN_READY;
        }

        break;
      case TRAIN_DISPATCH:
        Reply(tid, NULL, 0);
        circular_buffer_write_n(&cmd_buffer, (const unsigned char *) &req.cmd, sizeof(req.cmd));
    }

    // dispatch next command
    if (marklin_state == MARKLIN_READY && !circular_buffer_empty(&cmd_buffer)) {
      struct TrainCommand cmd;
      circular_buffer_read_n(&cmd_buffer, (unsigned char *) &cmd, sizeof(cmd));

      Putl(marklin_tx, req.cmd.data, req.cmd.len);
      marklin_state = MARKLIN_CMD_SENT;
    }
  }
}

int DispatchTrainCommand(int tid, const unsigned char *cmd, unsigned int len) {
  struct TrainDispatchRequest req = {.type = TRAIN_DISPATCH, .cmd = {.data = cmd, .len = len}};
  return Send(tid, (const char *) &req, sizeof(req), NULL, 0);
}
