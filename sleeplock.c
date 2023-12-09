#include "defs.h"

void
sleeplock_init(struct sleeplock *lk, char *name) {
    lk->owner = 0;
    lk->waiters.next = 0;
    spinlock_init(&lk->lk, name);
}

void
yield_to(struct tcb *thr) {
    struct tcb *me;
    me = mycore->thr;
    // Do not yield if I have become the owner.
    if (me == thr) return;
    // If the thread we yield to is in the signal handler,
    // this makes sure that we do not enable signals there.
    // There are only two places swtch() takes us to. One is
    // the below code, and the other is inside of scheduler().
    // Either of them has signals disabled.
    sig_disable();
    acquire(&mycore->lk);
    mycore->thr = thr;
    // Cannot switch to myself. (This happens when the owner changed between when you release the lock and when you call yeild_to()).
    swtch(&me->con, thr->con);
    release(&mycore->lk);
    sig_enable();
}

void
sleeplock_aquire(struct sleeplock *lk) {
    struct tcb *thr;
    struct waiter waiter;
    thr = mycore->thr;
    waiter.thr = thr;
    waiter.next = 0;
    acquire(&lk->lk);
    // No owner. I become the owner!
    if (!lk->owner) {
        lk->owner = mycore->thr;
        release(&lk->lk);
    } else {
        // Add calling thread to the waiting queue.
        waiter.next = lk->waiters.next;
        lk->waiters.next = &waiter;
        // Change the calling thread's state to SLEEPING.
        // So it's only scheduled after being assigned as the owner.
        // This is done with the lock held to avoid "lost wakeup."
        thr->state = SLEEPING;
        release(&lk->lk);
        // yield();
        yield_to(lk->owner);
    }
}

void
sleeplock_release(struct sleeplock *lk) {
    acquire(&lk->lk);
    if (!lk->waiters.next) {
        // No one's in the waiting queue. Set owner to 0.
        lk->owner = 0;
        release(&lk->lk);
    } else {
        // Assign the first waiter in the queue as the owner.
        struct tcb *thr = lk->waiters.next->thr;
        // Remove the first waiter from the queue.
        lk->waiters.next = lk->waiters.next->next;
        // Assign owner.
        lk->owner = thr;
        // Mark the new owner as runnable.
        thr->state = RUNNABLE;
        release(&lk->lk);
    }
}
