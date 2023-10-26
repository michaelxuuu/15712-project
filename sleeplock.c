#include "defs.h"

void
sleepspinlock_init(struct sleeplock *lk) {
    lk->owner = 0;
    lk->waiters.next = 0;
    spinlock_init(&lk->lk);
}

void
sleeplock_aquire(struct sleeplock *lk) {
    struct waiter waiter;
    struct thr *thr;
    thr = mycore->thr;
    waiter.thr = thr;
    waiter.next = 0;
    acquire(&lk->lk);
    // No owner. I become the owner!
    if (!lk->owner) {
        lk->owner = mycore->thr;
        release(&lk->lk);
    } else {
        // Enqueue
        waiter.next = lk->waiters.next;
        lk->waiters.next = &waiter;
        thr->state = SLEEPING;
        release(&lk->lk);
        // Make sure setting the state to RUNNABLE never happens
        // before setting state to SLEEPING (lost wakeup)
        // cmpxchg(&thr->state, RUNNING, SLEEPING);
        // Sleep
        dbg_printf("%s sleep\n", thr->name);
        pthread_kill(mycore->pthread, SIGUSR1);
    }
}

void
sleeplock_release(struct sleeplock *lk) {
    acquire(&lk->lk);
    if (!lk->waiters.next) {
        lk->owner = 0;
        release(&lk->lk);
    } else {
        // asm ("int $0x3");
        struct thr *thr = lk->waiters.next->thr;
        // Dequeue
        lk->waiters.next = lk->waiters.next->next;
        // Assign owner
        lk->owner = thr;
        thr->state = RUNNABLE;
        release(&lk->lk);
        // Make it runnable
        // thr->state = RUNNABLE;
    }
}
