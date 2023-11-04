#include "defs.h"

void
sigalrm_handler(int signo) {
    if (mycore != &g.cores[0]) {
        pthread_kill(g.cores[0].pthread, SIGALRM);
        return;
    }
    for (int i = 1; i < g.ncore; i++)
        pthread_kill(g.cores[i].pthread, SIGUSR1);
    yield(1);
}

void
sigusr1_handler(int signo, siginfo_t *siginfo, void *context) {
    yield(1);
}

void
sig_init() {
    struct sigaction sa_alrm;
    struct sigaction sa_usr1;

    memset(&sa_alrm, 0, sizeof(sa_alrm));
    sa_alrm.sa_flags = SA_RESTART;
    sa_alrm.sa_handler = sigalrm_handler;
    sigfillset(&sa_alrm.sa_mask);
    sigaction(SIGALRM, &sa_alrm, NULL);

    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_flags = SA_SIGINFO | SA_RESTART;
    sa_usr1.sa_sigaction = sigusr1_handler;
    sigfillset(&sa_usr1.sa_mask);
    sigaction(SIGUSR1, &sa_usr1, NULL);
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
