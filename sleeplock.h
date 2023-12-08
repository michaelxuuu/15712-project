struct waiter {
    struct tcb* thr;
    struct waiter* next;
};

struct sleeplock {
    struct waiter waiters;
    struct spinlock lk;
    struct tcb* owner;
};
