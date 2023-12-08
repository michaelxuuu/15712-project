struct tcb {
    char *name;
    struct tid id;
    int state;
    int sync;
    int joincnt;
    char* stack;
    void* status;
    void* arg;
    uthread_func func;
    struct core *core;
    struct context *con;
    struct tcb *next;
};
