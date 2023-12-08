#include "defs.h"

void
yield() {
    acquire(&mycore->lk);
    if (mycore->thr->state == RUNNING)
        mycore->thr->state = RUNNABLE;
    swtch(&mycore->thr->con, mycore->scheduler);
    release(&mycore->lk);
}

static void
start_uthread() {
    struct tcb *thr;
    release(&mycore->lk); // this does not enable siganls. we got here from scheduler() and we entered scheduler from signal handler where signals are disabled.
    sig_enable(); // there'll be no sigret helping us re-enable interrupts. thus, we need to re-enable interrupts ourselves
    thr = mycore->thr; // obtain the thread to execute from the core structure (save to do this before and after re-enabling signals)
    thr->status = thr->func(thr->arg); // exeucte the function and collect the return status
    thr->state = JOINABLE; // won't be scheduled agained
    yield(); // yield the cpu immediately, resource's collected when being joined
}

// main thread tcb
static struct tcb* thr0 = &(struct tcb){};

// core 0 scheduler stack
// Other cores will have their scheduler stack created by pthread_create()
static char stack0[STACKLEN] __attribute__((aligned(16)));

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

// Run by each core to initialize themselves
static void*
core_init(void* core){
    // Run by all cores
    mycore = (struct core*)core;
    mycore->id = mycore - &g.cores[0];
    mycore->thr = 0;
    mycore->term = 0;
    mycore->lkcnt = 0;
    mycore->thrs.next = 0;
    mycore->scheduler = (struct context*)0xdeadbeef;
    spinlock_init(&mycore->lk, "mycore.lk");
    // Run by core 0
    if (mycore == &g.cores[0]) {
        sig_init();
        timer_init();
        wrapper_init();
        // First thread being the thread that's running now
        thr0->name = "main";
        thr0->next = 0;
        thr0->core = core;
        thr0->state = RUNNABLE;
        thr0->joincnt = 0;
        thr0->sync = 0;
        // Core 0 is backed by the kernel scheduled thread
        // that's currently running
        mycore->thr = thr0;
        mycore->thrs.next = thr0;
        mycore->pthread = pthread_self();
        // This prepares to swtch() to scheduler() to start it
        mycore->scheduler = (struct context*)((stack0 + STACKLEN - 8) - sizeof(struct context));
        mycore->scheduler->rip = (unsigned long)scheduler;
        // Create other "cores"
        sig_disable(); // This will have other cores start with signals disabled
                        // Their siganls will be enabled unconditionally (in start_uthread()) 
                        // when their first thread is scheduled to run
        for (int i = 0; i < g.ncore-1; i++)
            pthread_create(&g.cores[i].pthread, 0, core_init, (void*)&g.cores[i]);
        // Use swtch() to switch to and start the scheduler (instead of yield()) to avoid deadlock
        // Normally yield() trigger by the sigalrm siganl would have scheduler() pick up from it left off
        // However, the *first* entry of scheduler() will have to start from the begining since it hasn't run yet
        // If we were to call yeild() deadlock would happen when the scheduler's acquiring mycore->lk
        // that yield() has already acquired
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

static void
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
    // Add to work queue
    acquire(&core->lk);
    thr->next = core->thrs.next;
    core->thrs.next = thr;
    release(&core->lk);
    // Return id to user
    thr->core = core;
    thr->id.thr = thr;
    thr->id.coreid = core->id;

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

void*
test(void *arg) {
    for (;;) {
        uthread_printf("%s", (char*)arg);
        // uthread_printf("");
    }
    return 0;
}

int main() {
    uthread_create(test, "1\n", "1");
    uthread_create(test, "2\n", "2");
    // uthread_create(test, "3", "3");
    for(;;);
}
