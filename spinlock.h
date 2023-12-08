struct spinlock {
    char *name;
    struct core* owner;
    int locked;
};
