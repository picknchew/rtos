#pragma once

#include "task.h"

void handle_exception(int exception);

int syscall_create(struct TaskDescriptor *parent, int priority, void (*code)());
int syscall_my_tid(struct TaskDescriptor *task);
int syscall_my_parent_tid(struct TaskDescriptor *task);
void syscall_yield();
void syscall_exit();
