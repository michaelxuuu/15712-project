#include "defs.h"

void
sigalrm_handler(int signo) {
    mycore->term++;
    struct core *nextcore = &g.cores[(mycore->id + 1) % g.ncore];
    if (nextcore != mycore && nextcore->term < mycore->term)
        pthread_kill(nextcore->pthread, SIGALRM);
    yield();
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

int
sig_disable() {
    sigset_t s1, s2;
    sigfillset(&s1);
    sigdelset(&s1, SIGKILL);
    sigdelset(&s1, SIGSTOP);
    pthread_sigmask(SIG_SETMASK, &s1, &s2);
    // Signals already disabled
    if (s2 == s1)
        return 0;
    return 1;
}

void
sig_enable() {
    sigset_t s;
    sigemptyset(&s);
    pthread_sigmask(SIG_SETMASK, &s, 0);
}

int
sig_pending() {
    sigset_t s;
    sigemptyset(&s);
    sigpending(&s);
    return sigismember(&s, SIGALRM);
}
