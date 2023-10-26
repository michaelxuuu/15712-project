struct core {
    int id;
    int lkcnt;
    int sigen;
    sigset_t sigset;
    struct thr thrs;
    struct thr *thr;
    struct spinlock lk;
    pthread_t pthread;
    struct context *scheduler;
};

