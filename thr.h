#define STACKLEN 4096

enum {
    RUNNING,
    RUNNABLE,
    SLEEPING,
    JOINABLE,
    JOINED
};

struct thrid {
    struct thr *thr;
    int coreid;
};

struct thr {
    char *name;
    struct thrid id;
    int state;
    int sync;
    int joincnt;
    char* stack;
    void* status;
    struct core *core;
    struct context *con;
    struct thr *next;
};

struct runtime {
    int init;
    int ncore;
    int nextcore;
    struct core* cores;
};

