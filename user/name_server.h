#define NAME_SERVER_NAME_MAX 16

void name_server_task();

enum NameServerRequestType {
    NAME_SERVER_WHOIS,
    NAME_SERVER_REGISTER_AS
};

struct RegisterAsRequest {
    char name[NAME_SERVER_NAME_MAX];
};

struct WhoisRequest {
    char name[NAME_SERVER_NAME_MAX];
};

struct NameServerRequest {
    enum NameServerRequestType req_type;

    union {
        struct RegisterAsRequest register_as_req;
        struct WhoisRequest whois_req;
    };
};

// interface for other tasks to call
int RegisterAs(const char *name);
int WhoIs(const char *name);
