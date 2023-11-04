struct core {
    int id;
    int lkcnt;
    int sigen;
    sigset_t sigset;
    struct tcb thrs;
    struct tcb *thr;
    struct spinlock lk;
    pthread_t pthread;
    struct context *scheduler;
};

