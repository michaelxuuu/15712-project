#include "defs.h"

void
sigalrm_handler(int signo) {
    mycore->term++;
    struct core *nextcore = &g.cores[(mycore->id + 1) % g.ncore];
    if (nextcore != mycore && nextcore->term < mycore->term)
        pthread_kill(nextcore->pthread, SIGALRM);
    mycore->sig_disable_cnt++;
    dbg_printf("%s entered hanlder\n", mycore->thr->name);
    yield();
    dbg_printf("%s exited from hanlder\n", mycore->thr->name);
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
    sigset_t s1, s2;
    sigfillset(&s1);
    sigdelset(&s1, SIGKILL);
    sigdelset(&s1, SIGSTOP);
    pthread_sigmask(SIG_SETMASK, &s1, &s2);
    mycore->sig_disable_cnt++;
}

void
sig_enable() {
    sigset_t s;
    sigemptyset(&s);
    mycore->sig_disable_cnt = 0;
    pthread_sigmask(SIG_SETMASK, &s, 0);
}
