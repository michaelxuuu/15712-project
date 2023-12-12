struct runtime {
    int init;
    int ncore;
    int nextcore;
    struct core* cores;
    atomic_uint readycnt;
};
