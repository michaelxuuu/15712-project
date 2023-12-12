#include "defs.h"

void
sigalrm_handler(int signo) {
    // Entering a signal hanlder has the same effect as to calling sig_disable().
    mycore->sig_disable_cnt++;
    mycore->term++;
    struct core *nextcore = &g.cores[(mycore->id + 1) % g.ncore];
    if (nextcore != mycore && nextcore->term < mycore->term && nextcore->pthread != 0)
        pthread_kill(nextcore->pthread, SIGALRM);
    yield();
    // Leaving a signal hanlder has the same effect as to calling sig_enable().
    mycore->sig_disable_cnt = 0;
}

void
sig_init() {
    struct sigaction sa_alrm;
    memset(&sa_alrm, 0, sizeof(sa_alrm));
    sa_alrm.sa_flags = SA_RESTART;
    sa_alrm.sa_handler = sigalrm_handler;
    sigfillset(&sa_alrm.sa_mask);
    sigaction(SIGALRM, &sa_alrm, NULL);
}

void
sig_disable() {
    sigset_t s;
    sigfillset(&s);
    // Optimize me?
    pthread_sigmask(SIG_SETMASK, &s, 0);
    mycore->sig_disable_cnt++;
}

void
sig_enable() {
    sigset_t s;
    sigemptyset(&s);
    mycore->sig_disable_cnt = 0;
    pthread_sigmask(SIG_SETMASK, &s, 0);
}
