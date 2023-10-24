#include "circular_buffer.h"

void circular_buffer_init(struct CircularBuffer *buffer) {
  buffer->size = 0;
  buffer->headIndex = 0;
  buffer->tailIndex = 0;
}

static bool is_full(const struct CircularBuffer *buffer) {
  return buffer->size == BUFFER_CAPACITY;
}

static void circular_buffer_write_item(
    struct CircularBuffer *buffer,
    union CircularBufferItem item) {
  if (is_full(buffer)) {
    return;
  }

  *(buffer->data + buffer->tailIndex) = item;
  buffer->tailIndex = (buffer->tailIndex + 1) % BUFFER_CAPACITY;
  ++buffer->size;
}

void circular_buffer_write(struct CircularBuffer *buffer, unsigned char ch) {
  union CircularBufferItem item = {.ch = ch};
  circular_buffer_write_item(buffer, item);
}

void circular_buffer_write_int8(struct CircularBuffer *buffer, uint8_t i) {
  union CircularBufferItem item = {.i = i};
  circular_buffer_write_item(buffer, item);
}

void circular_buffer_write_n(struct CircularBuffer *buffer, const unsigned char *data, unsigned int size) {
  for (int i = 0; i < size; ++i) {
    circular_buffer_write(buffer, data[i]);
  }
}

void circular_buffer_write_int64(struct CircularBuffer *buffer, uint64_t i) {
  // storing 64 bit int into 8 8-bit ints
  for (unsigned int byte = 0; byte < sizeof(uint64_t); ++byte) {
    // take last 8 bits of i
    circular_buffer_write_int8(buffer, (uint8_t) (i & 0xFF));
    i >>= 8;
  }
}

uint64_t circular_buffer_peek_int64(struct CircularBuffer *buffer) {
  uint64_t i = 0;

  // reconstruct 64 bit int by combining 8 8-bit ints
  for (int byte = sizeof(uint64_t) - 1; byte >= 0; --byte) {
    union CircularBufferItem item = buffer->data[(buffer->headIndex + byte) % BUFFER_CAPACITY];

    i = i << 8;
    i |= item.i;
  }

  return i;
}

size_t circular_buffer_size(struct CircularBuffer *buffer) {
  return buffer->size;
}

bool circular_buffer_empty(struct CircularBuffer *buffer) {
  return !buffer->size;
}

static union CircularBufferItem circular_buffer_read_item(struct CircularBuffer *buffer) {
  union CircularBufferItem item = *(buffer->data + buffer->headIndex);
  buffer->headIndex = (buffer->headIndex + 1) % BUFFER_CAPACITY;
  --buffer->size;

  return item;
}

char circular_buffer_read(struct CircularBuffer *buffer) {
  return circular_buffer_read_item(buffer).ch;
}

void circular_buffer_read_n(struct CircularBuffer *buffer, unsigned char *out, unsigned int size) {
  for (int i = 0; i < size; ++i) {
    out[i] = circular_buffer_read(buffer);
  }
}

uint8_t circular_buffer_read_int8(struct CircularBuffer *buffer) {
  return circular_buffer_read_item(buffer).i;
}

uint64_t circular_buffer_read_int64(struct CircularBuffer *buffer) {
  uint64_t i = circular_buffer_peek_int64(buffer);

  buffer->headIndex = (buffer->headIndex + sizeof(uint64_t)) % BUFFER_CAPACITY;
  buffer->size -= sizeof(uint64_t);

  return i;
}
