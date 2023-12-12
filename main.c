#include "uthread.h"

#include <sys/time.h>

int sync = 0;
#define THRCNT 10000
#define ITERNUM 1000000
uint64_t thrs[THRCNT] = {0};
uint64_t status[THRCNT] = {0};
int counters[THRCNT] = {0};

void*
test(void *arg) {
    int id = (int)arg;
    for (int i = 0; i < ITERNUM; i++)
        counters[id]++;
    return 0;
}

int main() {
    // Measure start time
    struct timeval start, end;
    gettimeofday(&start, NULL);

    char names[THRCNT][16];
    for(int i = 0; i < THRCNT; i++) {
        sprintf(names[i], "test%d", i);
        thrs[i] = uthread_create(test, (void *)i, names[i]);
    }
    for(int i = 0; i < THRCNT; i++)
        uthread_join(thrs[i], (void **)&status[i]);
    // for(;;);
    // Measure end time
    gettimeofday(&end, NULL);

    // Calculate elapsed time
    long long elapsed = (end.tv_sec - start.tv_sec) * 1000000LL + end.tv_usec - start.tv_usec;

    // Print results
    printf("Time taken: %lld microseconds\n", elapsed);

}

