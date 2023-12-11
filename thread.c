#include "defs.h"

// Yield control from the uthread running on "mycore" to the "mycore's" scheduler,
// which then finds and schedules the next runnable uthread.
void
yield() {
    acquire(&mycore->lk);
    dbg_printf("%s yields\n", mycore->thr->name);
    if (mycore->thr->state == RUNNING)
        mycore->thr->state = RUNNABLE;
    swtch(&mycore->thr->con, mycore->scheduler);
    // if it left of from signal handler
    // it would assume the sig_disable_cnt to be at least 2
    // however, it's possible it's not if yield() is not called from the signal hanlder but from start_uthread()
    // leading to the possiblility of sig_disable_cnt being 1, breaking the assumption
    // thus, siganls would be enabled from within the signal hanlder, resulting undefined behavior, but why???
    release(&mycore->lk);
}

// swtch() here from scheduler() when a uthread's scheduled for the first time.
static void
start_uthread() {
    struct tcb *thr = mycore->thr;
    release(&mycore->lk); // Release mycore->lk held from yield().
    sig_enable(); // Enable signals disabled upon entering sigalrm_handler().
    thr->status = thr->func(thr->arg); // Start the uthread and save the status returned.
    thr->state = JOINABLE; // Is it save to do without the lock held?
    sig_disable();
    yield(); // It's done running. Yield the CPU immediately.
}

// Uthread that runs main(). The main function is also abstracted into a uthread and participates
// in scheduling like all other uthreads.
static struct tcb* thr0 = &(struct tcb){};

// All softcores, except 0, have their scheduler stacks created by pthread_create().
// Thus, we need to prepare a scheduler stack for softcore 0 here.
static char stack0[STACKLEN] __attribute__((aligned(16)));

// This scheduler runs on a separate stack from the uthread stacks and signal stacks.
// In fact, signals use the same stacks as the uthread functions here since SA_ONSTACK
// is not set. Therefore, the scheduler has its own context. When a uThread's timer
// expires, it yields to the scheduler first. The scheduler then finds and schedules the next
// runnable uThread. So, every context switch involves:
// uthread 1 ---swtch()--> scheduler() ---swtch()--> uthread 2.
// This design is adopted from MIT's xv6 - a multi-core RISC-V kernel.
static void
scheduler() {
    for (;;) {
        
        acquire(&mycore->lk);
        for (struct tcb *thr = mycore->thrs.next; thr; thr = thr->next) {
            if (thr->state != RUNNABLE) continue;
            thr->state = RUNNING;
            mycore->thr = thr;
            dbg_printf("swtch to %s\n", thr->name);
            swtch(&mycore->scheduler, thr->con);
        }
        release(&mycore->lk);
    }
}

// Each softcore (pthread) runs this immediately after being spawned.
static void*
core_init(void* core){
    // Run by all cores
    mycore = (struct core*)core;
    mycore->id = mycore - &g.cores[0];
    mycore->thr = 0;
    mycore->term = 0;
    mycore->lkcnt = 0;
    mycore->thrs.next = 0;
    mycore->sig_disable_cnt = 0;
    mycore->scheduler = (struct context*)0xdeadbeef;
    spinlock_init(&mycore->lk, "mycore.lk");
    sig_disable();
    // Run by core 0
    if (mycore == &g.cores[0]) {
        sig_init();
        timer_init();
        wrapper_init();
        // Initialize the uthread that runs main().
        thr0->name = "main";
        thr0->next = 0;
        thr0->core = core;
        thr0->state = RUNNABLE;
        thr0->joincnt = 0;
        thr0->sync = 0;
        // Set thr0 as the currently running thread on core 0 and add it to the run queue.
        mycore->thr = thr0;
        mycore->thrs.next = thr0;
        // Core 0 is backed by the currently running pthread.
        mycore->pthread = pthread_self();
        // Create other "cores"
        // This will have other cores start with signals disabled
        // Their siganls will be enabled unconditionally (in start_uthread())
        // when their first thread is scheduled to run.
        sig_disable();
        for (int i = 0; i < g.ncore-1; i++)
            pthread_create(&g.cores[i].pthread, 0, core_init, (void*)&g.cores[i]);
        // Use swtch() to switch to and start the scheduler (instead of yield()) to avoid deadlock
        // Normally yield() trigger by the sigalrm siganl would have scheduler() pick up from it left off
        // However, the *first* entry of scheduler() will have to start from the begining since it hasn't run yet
        // If we were to call yeild() deadlock would happen when the scheduler's acquiring mycore->lk
        // that yield() has already acquired
        // Prepare the scheduler context to swtch() to.
        mycore->scheduler = (struct context*)((stack0 + STACKLEN - 8) - sizeof(struct context));
        mycore->scheduler->rip = (unsigned long)scheduler;
        swtch(&thr0->con, mycore->scheduler);
        release(&mycore->lk); // Relese the lock held by scheduler()
        // Now that scheduler() is properly initialized, and the next yield() called by handler will take us 
        // back to where scheduler() left off as expected, and we mustn't enable signals before
        // performing the above switch and would otherwise get into a deadlock if a timer signal
        // preceeds initializing the scheduler(), having us jump to executing scheduler() the first time
        // via yield(), resulting mycore->lk being attempted to be acquired a second time and hence a daedlock
        sig_enable(); // Somehow we must enable siganls before we set the timer or the signals would remain off strangely
        return 0;
    }
    // Enter scheduler() and NEVER return
    scheduler();
    return 0;
}

void
runtime_init() {
    g.init = 1;
    g.ncore = 1;
    g.nextcore = 0;
    // g.ncore = sysconf(_SC_NPROCESSORS_ONLN);
    g.cores = malloc(sizeof(struct core) * g.ncore);
    core_init(&g.cores[0]);
}

uint64_t
uthread_create(uthread_func func, void* arg, char* name) {
    if (!g.init) runtime_init();
    // Set up stack
    char *stack = (char*)mmap(NULL, STACKLEN, PROT_READ |
        PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    char *sp = stack + STACKLEN;
    // Put tcb at the top of the stack
    sp -= sizeof(struct tcb);
    struct tcb *thr = (struct tcb*)sp;
    thr->name = name;
    thr->stack = stack;
    thr->state = RUNNABLE;
    thr->sync = 0;
    thr->joincnt = 0;
    thr->func = func;
    thr->arg = arg;
    // align sp to 8 bytes, not 16, as if the function's reached via a call instruction.
    // the C standard mandates that sp should be aligned to 16 bytes before a call instruction.
    // thus, after call instruction pushed the return address onto the stack, sp should land on
    // a 8-byte boundary
    sp = (char *)(((uintptr_t)sp & ~15) - 8);
    thr->con = (struct context*)(sp -= sizeof(struct context));
    thr->con->rip = (unsigned long)start_uthread; // swtch() returns to start_uthread() from scheduler()
    // Pick a core
    int nextcore, nextnextcore;
    atomic_load_and_update(g.nextcore, nextcore, 
        nextnextcore, (nextcore + 1) % g.ncore);
    struct core *core = &g.cores[nextcore];

    // Return id to user
    thr->core = core;
    thr->id.thr = thr;
    thr->id.coreid = core->id;

    // Add to work queue
    acquire(&core->lk);
    thr->next = core->thrs.next;
    core->thrs.next = thr;
    release(&core->lk);

    return (uint64_t)&thr->id;
}

uint64_t
uthread_self(void) {
    return (uint64_t)mycore->thr;
}

int
uthread_join(uint64_t id, void* *statusptr) {
    struct tid *thrid = (struct tid *)id;
    int coreid = thrid->coreid;
    struct tcb *thr = thrid->thr;
    if (coreid > g.ncore || coreid < 0)
        return -1;
    struct core *core = &g.cores[thrid->coreid];
    acquire(&core->lk);
    struct tcb *p, *prev;
    for (p = core->thrs.next; p; prev = p, p = p->next) {
        if (p != thr) continue;
        thr->joincnt++;
        release(&core->lk);
        // Wait for thr to exit
        while (p->state != JOINABLE);
        // Dequeue
        acquire(&core->lk);
        if (p->state != JOINED)
            prev->next = p->next;
        p->state = JOINED;
        // Whoever called join() the last succeeds
        if (!--thr->joincnt) {
            *statusptr = p->status;
            release(&core->lk);
            return 0;
        } else {
            release(&core->lk);
            return -1;
        }
    }
    release(&core->lk);
    return 1;
}

void
uthread_exit(void *status) {
    mycore->thr->status = status;
    mycore->thr->state = JOINABLE;
    yield();
}
