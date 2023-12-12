#define _GNU_SOURCE
#include "defs.h"
#include "uthread.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <sched.h>  // Include the sched.h header for CPU_ZERO and related macros

#define NTHR 10000
#define NITER 100000

static struct {
    uint64_t uthrs[NTHR];
    pthread_t pthrs[NTHR];
    atomic_uint fincnt;
    int counters[NTHR];
    struct timeval start, end;
    long long elapsed;
} test = { .fincnt = 0 };

void*
thrfunc(void *arg) {
    int id = (int)arg;
    for (int i = 0; i < NITER; i++)
        test.counters[id]++;
    test.fincnt++;
    test.counters[id] = 0;
    return 0;
}

void timstart() {
    gettimeofday(&test.start, NULL);
}

void timend() {
    gettimeofday(&test.end, NULL);
    test.elapsed = (test.end.tv_sec - test.start.tv_sec) * 1000000LL + test.end.tv_usec - test.start.tv_usec;
}

void pcreate() {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // cpu_set_t cpuset;
    // CPU_ZERO(&cpuset);
    // CPU_SET(0, &cpuset);
    // if (pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset) != 0) {
    //     perror("pthread_attr_setaffinity_np");
    //     exit(EXIT_FAILURE);
    // }
    for(int i = 0; i < NTHR; i++)
        if(pthread_create(&test.pthrs[i], &attr, thrfunc, 0)) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
}

void pjoin() {
    void *status;
    for(int i = 0; i < NTHR; i++) {
        assert(!pthread_join(test.pthrs[i], &status));
        // printf("joined %d\n", i);
    }
}

void ucreate() {
    for(int i = 0; i < NTHR; i++)
        if (uthread_create(&test.uthrs[i], thrfunc, (void *)i, "")) {
            perror("uthread_create");
            exit(EXIT_FAILURE);
        }
}

void ujoin() {
    void *status;
    for(int i = 0; i < NTHR; i++) {
        assert(!uthread_join(test.uthrs[i], &status));
        // debug_printf("joined %d\n", i);
    }
}

void
benchmark_uthread(int join) {
    if (join) {
        timstart();
        ucreate();
        ujoin();
        timend();
    } else {
        timstart();
        ucreate();
        timend();
        ujoin();
    }
}


void
benchmark_pthread(int join) {
    if (join) {
        timstart();
        pcreate();
        pjoin();
        timend();
    } else {
        timstart();
        pcreate();
        timend();
        pjoin();
    }

}

void runtime_init();
int main() {
    benchmark_pthread(1);
    printf("%lld\n", test.elapsed);
    benchmark_uthread(1);
    // runtime_init();
    // for(;;);
    printf("%lld\n", test.elapsed);
}

