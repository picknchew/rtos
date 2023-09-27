#pragma once

#include "rpi.h"
#include <stdarg.h>
#include "task.h"

#define DEBUG 0

// NOTE: completely clobbers registers
void debug_dump_registers() {
  #if DEBUG
  register int x0 asm ("x0");
  register int x1 asm ("x1");
  register int x2 asm ("x2");
  register int x3 asm ("x3");
  register int x4 asm ("x4");
  register int x5 asm ("x5");
  register int x6 asm ("x6");
  register int x7 asm ("x7");
  register int x8 asm ("x8");
  register int x9 asm ("x9");
  register int x10 asm ("x10");
  register int x11 asm ("x11");
  register int x12 asm ("x12");
  register int x13 asm ("x13");
  register int x14 asm ("x14");
  register int x15 asm ("x15");
  register int x16 asm ("x16");
  register int x17 asm ("x17");
  register int x18 asm ("x18");
  register int x19 asm ("x19");
  register int x20 asm ("x20");
  register int x21 asm ("x21");
  register int x22 asm ("x22");
  register int x23 asm ("x23");
  register int x24 asm ("x24");
  register int x25 asm ("x25");
  register int x26 asm ("x26");
  register int x27 asm ("x27");
  register int x28 asm ("x28");
  register int x29 asm ("x29");
  register int x30 asm ("x30");

  uart_printf(1, "REGISTERS:\r\n"
  "x0: %d\r\n"
  "x1: %d\r\n"
  "x2: %d\r\n"
  "x3: %d\r\n"
  "x4: %d\r\n"
  "x5: %d\r\n"
  "x6: %d\r\n"
  "x7: %d\r\n"
  "x8: %d\r\n"
  "x9: %d\r\n"
  "x10: %d\r\n"
  "x11: %d\r\n"
  "x12: %d\r\n"
  "x13: %d\r\n"
  "x14: %d\r\n"
  "x15: %d\r\n"
  "x16: %d\r\n"
  "x17: %d\r\n"
  "x18: %d\r\n"
  "x19: %d\r\n"
  "x20: %d\r\n"
  "x21: %d\r\n"
  "x22: %d\r\n"
  "x23: %d\r\n"
  "x24: %d\r\n"
  "x25: %d\r\n"
  "x26: %d\r\n"
  "x27: %d\r\n"
  "x28: %d\r\n"
  "x29: %d\r\n"
  "x30: %d\r\n", x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30);

  struct TaskDescriptor *current_task = task_get_current_task();
  printf("x0: %d, x1: %d, x2: %d, x3: %d x4: %d x5: %d\r\n", current_task->context.registers[0], current_task->context.registers[1], current_task->context.registers[2], current_task->context.registers[3], current_task->context.registers[4], current_task->context.registers[5]);
  #endif
}

void debug_dump_context() {
  #if DEBUG
  struct TaskContext *context = &task_get_current_task()->context;

  uint64_t x0 =  context->registers[0];
  uint64_t x1 =  context->registers[1];
  uint64_t x2 =  context->registers[2];
  uint64_t x3 =  context->registers[3];
  uint64_t x4 =  context->registers[4];
  uint64_t x5 =  context->registers[5];
  uint64_t x6 =  context->registers[6];
  uint64_t x7 =  context->registers[7];
  uint64_t x8 =  context->registers[8];
  uint64_t x9 =  context->registers[9];
  uint64_t x10 = context->registers[10];
  uint64_t x11 = context->registers[11];
  uint64_t x12 = context->registers[12];
  uint64_t x13 = context->registers[13];
  uint64_t x14 = context->registers[14];
  uint64_t x15 = context->registers[15];
  uint64_t x16 = context->registers[16];
  uint64_t x17 = context->registers[17];
  uint64_t x18 = context->registers[18];
  uint64_t x19 = context->registers[19];
  uint64_t x20 = context->registers[20];
  uint64_t x21 = context->registers[21];
  uint64_t x22 = context->registers[22];
  uint64_t x23 = context->registers[23];
  uint64_t x24 = context->registers[24];
  uint64_t x25 = context->registers[25];
  uint64_t x26 = context->registers[26];
  uint64_t x27 = context->registers[27];
  uint64_t x28 = context->registers[28];
  uint64_t x29 = context->registers[29];
  uint64_t x30 = context->registers[30];

  printf("CURRENT CONTEXT:\r\n"
  "x0: %d\r\n"
  "x1: %d\r\n"
  "x2: %d\r\n"
  "x3: %d\r\n"
  "x4: %d\r\n"
  "x5: %d\r\n"
  "x6: %d\r\n"
  "x7: %d\r\n"
  "x8: %d\r\n"
  "x9: %d\r\n"
  "x10: %d\r\n"
  "x11: %d\r\n"
  "x12: %d\r\n"
  "x13: %d\r\n"
  "x14: %d\r\n"
  "x15: %d\r\n"
  "x16: %d\r\n"
  "x17: %d\r\n"
  "x18: %d\r\n"
  "x19: %d\r\n"
  "x20: %d\r\n"
  "x21: %d\r\n"
  "x22: %d\r\n"
  "x23: %d\r\n"
  "x24: %d\r\n"
  "x25: %d\r\n"
  "x26: %d\r\n"
  "x27: %d\r\n"
  "x28: %d\r\n"
  "x29: %d\r\n"
  "x30: %d\r\n", x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30);
  #endif
}

void debug_printf(char* fmt, ...) {
  #if DEBUG
  va_list va;

  va_start(va, fmt);
  uart_format_print(1, fmt, va);
  va_end(va);
  #endif
}
