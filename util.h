#pragma once

#include <stdbool.h>
#include <stddef.h>

// conversions
int a2d(char ch);
char a2i(char ch, char **src, int base, int *nump);
void ui2a(unsigned int num, unsigned int base, char *bf);
void i2a(int num, char *bf);

// memory
void *memset(void *s, int c, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);

int min(int a, int b);
int strcmp(char *str1, char *str2);
int strlen(const char *str);
bool is_number(char *str);
int atoi(char *str);
char *strtok_r(char *str, char delim, char **saveptr);
char *strcat (char *dest, const char *src,int len);
