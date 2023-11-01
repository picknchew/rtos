#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BUFFER_CAPACITY 1024

union CircularBufferItem {
  unsigned char ch;
  uint8_t i;
};

struct CircularBuffer {
  union CircularBufferItem data[BUFFER_CAPACITY];
  size_t size;
  unsigned int headIndex;
  unsigned int tailIndex;
};

void circular_buffer_init(struct CircularBuffer *buffer);
void circular_buffer_write(struct CircularBuffer *buffer, unsigned char ch);
void circular_buffer_write_n(
    struct CircularBuffer *buffer,
    const unsigned char *data,
    unsigned int size);
void circular_buffer_write_int8(struct CircularBuffer *buffer, uint8_t i);
void circular_buffer_write_int64(struct CircularBuffer *buffer, uint64_t i);
uint64_t circular_buffer_peek_int64(struct CircularBuffer *buffer);
char circular_buffer_read(struct CircularBuffer *buffer);
void circular_buffer_read_n(struct CircularBuffer *buffer, unsigned char *out, unsigned int size);
uint8_t circular_buffer_read_int8(struct CircularBuffer *buffer);
uint64_t circular_buffer_read_int64(struct CircularBuffer *buffer);
bool circular_buffer_empty(struct CircularBuffer *buffer);
size_t circular_buffer_size(struct CircularBuffer *buffer);
