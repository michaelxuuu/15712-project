struct waiter {
    struct tcb* thr;
    struct waiter* next;
};

struct sleeplock {
    struct waiter waiters;
    struct spinlock lk;
    struct tcb* owner;
    struct core *ownercore; // This is necessary in case the owner exits and becomes joined with the lock held, 
                    // rendering the "owner" field no longer valid. Any attempts to access core information through "owner" 
                    // (as in sleeplock_acquire()) would result in undefined behavior.
};
