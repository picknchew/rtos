#include "util.h"

#include <stdbool.h>
#include <stdint.h>

// ascii digit to integer
int a2d(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

// unsigned int to ascii string
void ui2a(unsigned int num, unsigned int base, char* bf) {
  int n = 0;
  int dgt;
  unsigned int d = 1;

  while ((num / d) >= base) {
    d *= base;
  }
  while (d != 0) {
    dgt = num / d;
    num %= d;
    d /= base;
    if (n || dgt > 0 || d == 0) {
      *bf++ = dgt + (dgt < 10 ? '0' : 'a' - 10);
      ++n;
    }
  }
  *bf = 0;
}

// signed int to ascii string
void i2a(int num, char* bf) {
  if (num < 0) {
    num = -num;
    *bf++ = '-';
  }
  ui2a(num, 10, bf);
}

// define our own memset to avoid SIMD instructions emitted from the compiler
void* memset(void* s, int c, size_t n) {
  for (char* it = (char*) s; n > 0; --n) {
    *it++ = c;
  }
  return s;
}

// define our own memcpy to avoid SIMD instructions emitted from the compiler
void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
  if (!dest || !src) {
    return NULL;
  }

  char* sit = (char*) src;
  char* cdest = (char*) dest;

  for (size_t i = 0; i < n; ++i) {
    *(cdest++) = *(sit++);
  }
  return dest;
}

int min(int a, int b) {
  return (a < b) ? a : b;
}

int strcmp(char* str1, char* str2) {
  while (*str1 && *str2) {
    if (*str1 != *str2) {
      return 0;
    }

    ++str1;
    ++str2;
  }

  return !(*str1) && !(*str2);
}

int strlen(const char* str) {
  int index = 0;

  while (str[index]) {
    ++index;
  }

  return index;
}

bool is_number(char* str) {
  int len = strlen(str);

  for (int i = 0; i < len; ++i) {
    if (str[i] < '0' || str[i] > '9') {
      return false;
    }
  }

  return true;
}

int atoi(char* str) {
  int ret = 0;
  int len = strlen(str);

  for (int i = 0; i < len; ++i) {
    ret *= 10;
    ret += (str[i] - '0');
  }

  return ret;
}

char* strtok_r(char* str, char delim, char** saveptr) {
  char* cur = str;

  if (!str) {
    str = *saveptr;
    cur = *saveptr;
  }

  if (!cur || *cur == '\0') {
    return NULL;
  }

  while (*cur) {
    if (*cur == delim) {
      *cur = '\0';
      *saveptr = cur + 1;
      return str;
    }

    ++cur;
  }

  *saveptr = NULL;
  return str;
}

uint64_t rand() {
  static uint64_t seed = 77;

  seed ^= (seed << 21);
  seed ^= (seed >> 35);
  seed ^= (seed << 4);

  return seed;
}
