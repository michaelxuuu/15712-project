struct core {
    int id;
    int term;
    int lkcnt;
    int sig_disable_cnt;
    int sigen_nolk;
    sigset_t sigset;
    struct tcb thrs;
    struct tcb *thr;
    struct spinlock lk;
    pthread_t pthread;
    struct context *scheduler;
};

