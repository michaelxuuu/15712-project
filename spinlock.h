struct spinlock {
    struct core* owner;
    int locked;
};
