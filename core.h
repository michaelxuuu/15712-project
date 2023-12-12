struct core {
    int id;
    int term;
    int lkcnt;
    int sig_disable_cnt; // This records the number of times the signal is disabled. A counter is needed than a binary flag so that 
                        // we get to know whether the interrupt have enabled before we call sig_disable() in spinlk_acquire().
                        // This counter gets reset to 0 by calling sig_enable().
    int sigen_nolk;  // Signal Enabled No Lock: records whether the signal is enabled when no locks are held.
                    // This flag is checked when the last spin lock is released. 
                    // And if the flag is set, interrupts must be re-enabled when the last spin lock held by this core is released.
    sigset_t sigset;
    struct tcb thrs;
    struct tcb joined_thrs;
    struct tcb *thr;
    struct spinlock lk;
    pthread_t pthread;
    struct context *scheduler;
};

