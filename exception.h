#pragma once

#include <stdint.h>

#include "task.h"

void handle_exception(uint64_t exception);

// tasks
int syscall_create(struct TaskDescriptor *parent, int priority, void (*code)());
int syscall_my_tid(struct TaskDescriptor *task);
int syscall_my_parent_tid(struct TaskDescriptor *task);
void syscall_yield();
void syscall_exit();

// message passing
int syscall_send(struct TaskDescriptor *sender, int tid, const char *msg, int msglen, char *reply, int rplen);
int syscall_receive(struct TaskDescriptor *receiver, int *tid, char *msg, int msglen);
int syscall_reply(struct TaskDescriptor *receiver,int tid, const char *reply, int rplen);
int syscall_await_event(int event_id);
