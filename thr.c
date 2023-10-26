#include "defs.h"

struct runtime g = { 
    .init = 0,
};

__thread struct core* mycore;

// swtch() here from scheduler() the first time a thread's sched'ed
// This sigret() is designed to bypass the kernel's sigret()
// When a thread's sched'ed for the first time, it doesn't have the signal stack
// set up by the kernel, so it couldn't and shouldn't "resumes" via the kernel's
// sigret(), and this sigret's designed to be used in replace of the kernel's sigret()
// to "resume" a thread from a signal handler. For the same reason, we need to enable
// signals on this core manually, which would've have been taken care of by the kernel
// if the kernel's sigret() is used.
void
sigret() {
    release(&mycore->lk);
    sig_enable();
    // Return to func() (prepared on the thread's stack during creation)
}

// thr goes here after it finishes
void
cleanup() {
    register uint64_t rax asm("rax");
    mycore->thr->status = (void*)rax;
    mycore->thr->state = JOINABLE;
    for(;;);
}

static char stack0[STACKLEN] __attribute__((aligned(16)));

void
scheduler() {
    for (;;) {
        acquire(&mycore->lk);
        for (struct thr *thr = mycore->thrs.next; thr; thr = thr->next) {
            if (thr->state != RUNNABLE) continue;
            thr->state = RUNNING;
            mycore->thr = thr;
            dbg_printf("schedule %s\n", thr->name);
            swtch(&mycore->scheduler, thr->con);
        }
        release(&mycore->lk);
    }
}

// siguer1_hldr() goes here, yield to scheduler()
void
yield() {
    acquire(&mycore->lk);
    if (mycore->thr->state == RUNNING)
        mycore->thr->state = RUNNABLE;
    swtch(&mycore->thr->con, mycore->scheduler);
    release(&mycore->lk);
}

void timer_init() {
    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_sec = 1;
    timer.it_interval.tv_sec = 1;
    // timer.it_value.tv_usec = 1000 * 10;
    // timer.it_interval.tv_usec = 1000 * 10;
    setitimer(ITIMER_REAL, &timer, NULL);
}

// Run by each core to initialize themselves
void*
core_init(void* core){
    // Run by all cores
    mycore = (struct core*)core;
    mycore->id = mycore - &g.cores[0];
    mycore->thr = 0;
    mycore->lkcnt = 0;
    mycore->thrs.next = 0;
    mycore->scheduler = (struct context*)0xdeadbeef;
    spinlock_init(&mycore->lk);
    // Run by core 0
    if (mycore == &g.cores[0]) {
        sig_init();
        timer_init();
        // First thread being the thread that's running now
        struct thr *thr;
        thr = malloc(sizeof(struct thr));
        thr->name = "t0";
        thr->next = 0;
        thr->core = core;
        thr->state = RUNNABLE;
        thr->joincnt = 0;
        thr->sync = 0;
        // Core 0 is backed by the kernel scheduled thread
        // that's currently running
        mycore->thr = thr;
        mycore->thrs.next = thr;
        mycore->pthread = pthread_self();
        // This prepares to swtch() to scheduler() to start it
        mycore->scheduler = (struct context*)((stack0 + STACKLEN - 8) - sizeof(struct context));
        mycore->scheduler->rip = (unsigned long)scheduler;
        // Create other "cores"
        sig_disable(); // This will have other cores start with signals disabled
                        // Their siganls will be enabled unconditionally (in sigret()) 
                        // when their first thread is scheduled to run
        for (int i = 0; i < g.ncore-1; i++)
            pthread_create(&g.cores[i].pthread, 0, core_init, (void*)&g.cores[i]);
        // Use swtch() to switch to and start the scheduler (instead of yield()) to avoid deadlock
        // Normally yield() trigger by the sigalrm siganl would have scheduler() pick up from it left off
        // However, the *first* entry of scheduler() will have to start from the begining since it hasn't run yet
        // If we were to call yeild() deadlock would happen when the scheduler's acquiring mycore->lk
        // that yield() has already acquired
        swtch(&thr->con, mycore->scheduler);
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
thr_create(void*(*func)(), void* arg, char* name) {
    if (!g.init) runtime_init();
    struct thr *thr = _malloc(sizeof(struct thr));
    thr->name = name;
    thr->stack =  mmap(NULL, STACKLEN, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    thr->state = RUNNABLE;
    thr->sync = 0;
    thr->joincnt = 0;
    // Set up stack
    char *sp = thr->stack + STACKLEN;
    *(void* *)(sp -= sizeof(void*)) = arg;
    *(void* *)(sp -= sizeof(void*)) = cleanup;
    *(void* *)(sp -= sizeof(void*)) = doret1;
    *(void* *)(sp -= sizeof(void*)) = func;
    *(void* *)(sp -= sizeof(void*)) = doret2;
    thr->con = (struct context*)(sp -= sizeof(struct context));
    thr->con->rip = (unsigned long)sigret; // swtch() returns to sigret() from scheduler()
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
thr_self(void) {
    return (uint64_t)mycore->thr;
}

int
thr_join(uint64_t id, void* *statusptr) {
    struct thrid *thrid = (struct thrid *)id;
    int coreid = thrid->coreid;
    struct thr *thr = thrid->thr;
    if (coreid > g.ncore || coreid < 0)
        return -1;
    struct core *core = &g.cores[thrid->coreid];
    acquire(&core->lk);
    struct thr *p, *prev;
    for (struct thr *p = core->thrs.next; p; prev = p, p = p->next) {
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
            _free(p);
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
thr_exit(void *status) {
    mycore->thr->status = status;
    mycore->thr->state = JOINABLE;
    for(;;);
}

void*
test(void *arg) {
    for (;;) {
        _printf("%s\n", (char*)arg);
    }
    return 0;
}

int main() {
    thr_create(test, "t1", "1");
    thr_create(test, "t2", "2");
    thr_create(test, "t3", "3");
    for(;;);
}
