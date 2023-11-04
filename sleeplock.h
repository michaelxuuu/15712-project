#define SLEEPSPINLOCK_INIT_LIST {   \
    .lk = 0,                        \
    .owner = 0,                     \
    .waiters.next = 0               \
}

struct waiter {
    struct tcb* thr;
    struct waiter* next;
};

struct sleeplock {
    struct waiter waiters;
    struct spinlock lk;
    struct tcb* owner;
};
