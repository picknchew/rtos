#pragma once

#include <stdint.h>

#include "task.h"

void handle_exception(uint64_t exception);

int syscall_create(struct TaskDescriptor *parent, int priority, void (*code)());
int syscall_my_tid(struct TaskDescriptor *task);
int syscall_my_parent_tid(struct TaskDescriptor *task);
void syscall_yield();
void syscall_exit();
