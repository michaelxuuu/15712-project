#include "defs.h"

void
spinlock_init(struct spinlock *lk, char *name) {
    lk->name = name;
    lk->owner = 0;
    lk->locked = 0;
}

// Note this function also disable signals on the softcore on which it's called.
void
acquire(struct spinlock *lk) {
    // Disable signals on this core
    int sigen = sig_disable();
    // Record signal status when no locks held
    if (!mycore->lkcnt)
        mycore->sigen = sigen;
    // Increment lock count
    mycore->lkcnt++;
    while (xchg(&lk->locked, 1));
    // Record ownership
    lk->owner = mycore;
}

void
release(struct spinlock *lk){
    lk->owner = 0;
    asm volatile("movl $0, %0" : "+m"(lk->locked));
    // If signals were initially off, we leave them off.
    if (!--mycore->lkcnt && mycore->sigen)
        sig_enable();
}
