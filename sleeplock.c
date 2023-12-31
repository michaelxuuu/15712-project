#include "defs.h"

void
sleeplock_init(struct sleeplock *lk, char *name) {
    lk->owner = 0;
    lk->ownercore = 0;
    lk->waiters.next = 0;
    spinlock_init(&lk->lk, name);
}

// This yield implementation does not go through the scheduler; instead, it enables
// a uthread to defer some of its time quota to another uthread that holds the resources
// needed for this uthread to make progress.
// If we are here it's guranteed that the old owner is on the same core as the calling thread!
static void
yield_to(struct tcb *oldowner, struct sleeplock *lk) {
    struct tcb *me;
    struct tcb *newowner;
    me = mycore->thr;
    // Do not yield if I have become the owner.
    // Cannot switch to myself. (This happens when the owner changed between when you release the lock and when you call yeild_to()).
    if (lk->owner != oldowner) return;
    // If the thread we yield to is in the signal handler,
    // this makes sure that we do not enable signals there.
    // There are only two places swtch() takes us to. One is
    // the below code, and the other is inside of scheduler().
    // Either of them has signals disabled.
    sig_disable();
    acquire(&mycore->lk);
    // Check again if the owner has changed.
    if (lk->owner != oldowner) return;
    mycore->thr = oldowner;
    swtch(&me->con, oldowner->con);
    release(&mycore->lk);
    sig_enable();
}

void
sleeplock_aquire(struct sleeplock *lk) {
    struct tcb *thr;
    struct tcb *oldowner;
    struct waiter waiter;
    thr = mycore->thr;
    waiter.thr = thr;
    waiter.next = 0;
    acquire(&lk->lk);
    assert(mycore->thr != lk->owner); // Not a re-entrant lock!
    // No owner. I become the owner!
    if (!lk->owner) {
        lk->owner = mycore->thr;
        lk->ownercore = mycore;
        release(&lk->lk);
    } else {
        // Add calling thread to the waiting queue.
        waiter.next = lk->waiters.next;
        lk->waiters.next = &waiter;
        if (lk->ownercore == mycore) {
            // Change the calling thread's state to SLEEPING.
            // So it's only scheduled after being assigned as the owner.
            // This is done with the lock held to avoid "lost wakeup."
            thr->state = SLEEPING;
            oldowner = lk->owner;
            release(&lk->lk);
            // If the state is not set to SLEEPING, the scheduler can come in here
            // and schedule this thread.
            yield_to(oldowner, lk);
        } else {
            // Release the spinlock and wait to be assigned as the owner.
            // This is when the sleeplock downgrades into a spinlock, especially if the owner is on a different core.
            // If the owner exits with the lock held on another core, the calling thread would spin forever.
            release(&lk->lk);
            while (lk->owner != mycore->thr);
        }
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
