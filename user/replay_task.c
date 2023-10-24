#include "replay_task.h"

#include <stdbool.h>

#include "../circular_buffer.h"
#include "../syscall.h"
#include "../train/train_dispatcher.h"
#include "../uart.h"
#include "io_server.h"
#include "name_server.h"

#define TRAINSET_NUM_FEEDBACK_MODULES 5
#define TRAINSET_NUM_SENSORS_PER_MODULE 16

// static const int CMD_READ_ALL_SENSORS = 0x80 + TRAINSET_NUM_FEEDBACK_MODULES;
static const unsigned char CMD_READ_ALL_SENSORS[] = {0x80 + TRAINSET_NUM_FEEDBACK_MODULES};

static void read_sensor_data(int train_dispatcher) {
  DispatchTrainCommand(train_dispatcher, CMD_READ_ALL_SENSORS, 1);
}

static void process_sensor_data(struct CircularBuffer *buffer, bool *sensors_occupied) {
  // each feedback module has 2 numbers (contacts 1 to 8) and (contacts 9 to 16)
  for (int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * 2; ++i) {
    char ch = circular_buffer_read(buffer);

    // 1 byte (8 bits) and each char represents 8 sensors
    // the most significant bit represents the lowest sensor
    for (int j = 0; j < 8; ++j) {
      // take last bit each time
      // trainset->sensors_occupied[i * 8 + j] = (ch >> j) & 1;
      sensors_occupied[i * 8 + j] = (ch >> (7 - j)) & 1;
    }
  }
}

static void update_screen(bool *sensors) {
  printf("Recently triggered sensors:");

  for (unsigned int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * TRAINSET_NUM_SENSORS_PER_MODULE;
       ++i) {
    if (sensors[i]) {
      char ch = 'A' + i / 16;
      printf(" %c%d", ch, i % 16 + 1);
    }
  }

  printf("\r\n");
}

static const int LINE_TRAINSET = 2;
static const int BAUD_RATE = 2400;

void replay_task() {
  uart_config_and_enable(LINE_TRAINSET, BAUD_RATE, true, true);
  int console_rx = WhoIs("console_io_rx");
  int console_tx = WhoIs("console_io_tx");
  int marklin_tx = WhoIs("marklin_io_tx");
  int marklin_rx = WhoIs("marklin_io_rx");
  int train_dispatch = WhoIs("train_dispatch");

  struct CircularBuffer rx_buffer;
  circular_buffer_init(&rx_buffer);
  bool sensors[TRAINSET_NUM_FEEDBACK_MODULES * TRAINSET_NUM_SENSORS_PER_MODULE];

  while (true) {
    char ch = Getc(console_rx);

    Putc(console_tx, ch);

    printf("requesting train data\r\n");
    read_sensor_data(train_dispatch);

    printf("waiting for train data\r\n");
    for (int i = 0; i < TRAINSET_NUM_FEEDBACK_MODULES * 2; ++i) {
      char marklin_ch = Getc(marklin_rx);
      circular_buffer_write(&rx_buffer, marklin_ch);
    }

    printf("got train data!\r\n");
    process_sensor_data(&rx_buffer, sensors);

    update_screen(sensors);
  }

  Exit();
}
