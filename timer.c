#include "defs.h"

void
timer_init() {
    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    // timer.it_value.tv_sec = 1;
    // timer.it_interval.tv_sec = 1;
    timer.it_value.tv_usec = 1000;
    timer.it_interval.tv_usec = 1000;
    setitimer(ITIMER_REAL, &timer, NULL);
}
