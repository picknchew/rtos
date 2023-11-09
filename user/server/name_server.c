#include "name_server.h"

#include <stdbool.h>

#include "rpi.h"
#include "syscall.h"
#include "task.h"
#include "util.h"

struct NameEntry {
  char name[NAME_SERVER_NAME_MAX];
};

// indexed by tid
static int name_server_tid;
static struct NameEntry name_map[TASKS_MAX];

static void name_map_init() {
  name_server_tid = -1;

  for (int i = 0; i < TASKS_MAX; ++i) {
    memset(name_map[i].name, 0, NAME_SERVER_NAME_MAX);
  }
}

static int name_map_get(char *name) {
  for (int i = 0; i < TASKS_MAX; ++i) {
    if (strcmp(name_map[i].name, name)) {
      return i;
    }
  }

  return -1;
}

static void name_map_clear_entry(int tid) {
  memset(name_map[tid].name, 0, NAME_SERVER_NAME_MAX);
}

void name_server_task() {
  name_map_init();

  name_server_tid = MyTid();

  while (true) {
    int tid;
    struct NameServerRequest req = {.req_type = NAME_SERVER_REGISTER_AS};

    Receive(&tid, (char *) &req, sizeof(req));

    int response = 0;
    int existing_tid;

    switch (req.req_type) {
      case NAME_SERVER_WHOIS:
        response = name_map_get(req.whois_req.name);
        break;
      case NAME_SERVER_REGISTER_AS:
        existing_tid = name_map_get(req.register_as_req.name);

        if (existing_tid != -1) {
          name_map_clear_entry(existing_tid);
        }

        // printf("name_server: registered %d as %s\r\n", tid, req.register_as_req.name);

        memcpy(name_map[tid].name, req.register_as_req.name, NAME_SERVER_NAME_MAX);

        response = 0;
        break;
      default:
        printf("name_server: received invalid request!\r\n");
    }

    Reply(tid, (const char *) &response, sizeof(response));
  }

  Exit();
}

int RegisterAs(const char *name) {
  if (name_server_tid == -1) {
    return -1;
  }

  struct NameServerRequest req = {.req_type = NAME_SERVER_REGISTER_AS};
  int response;

  memcpy(req.register_as_req.name, name, NAME_SERVER_NAME_MAX);
  Send(name_server_tid, (const char *) &req, sizeof(req), (char *) &response, sizeof(response));
  return response;
}

int WhoIs(const char *name) {
  if (name_server_tid == -1) {
    return -1;
  }

  struct NameServerRequest req = {.req_type = NAME_SERVER_WHOIS};
  int tid;

  memcpy(req.whois_req.name, name, NAME_SERVER_NAME_MAX);
  Send(name_server_tid, (const char *) &req, sizeof(req), (char *) &tid, sizeof(tid));

  return tid;
}
