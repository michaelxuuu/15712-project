#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/time.h>
#include <sys/mman.h>

// Order matters here
#include "spinlock.h"
#include "sleeplock.h"
#include "con.h"
#include "thr.h"
#include "core.h"

struct thr;
struct core;
struct thrid;
struct context;
struct runtime;
struct spinlock;
struct sleeplock;

// spinlock.c
void acquire(struct spinlock*);
void release(struct spinlock*);
void spinlock_init(struct spinlock*);

// sleeplock.c
void sleepspinlock_init(struct sleeplock*);
void sleeplock_aquire(struct sleeplock*);
void sleeplock_release(struct sleeplock*);

// wrapper.c
void* _malloc(size_t size);
void _free(void* ptr);
void _printf(const char* fmt, ...);

// asm.S
void doret1();
void doret2();
void sigret();
int xchg(void* p, int new);
int cmpxchg(void* p, int old, int new);

// thr.c
extern struct runtime g;
extern __thread struct core* mycore;
void yield();
void swtch(struct context**, struct context*);

// sig.c
void sig_init();
int sig_disable();
void sig_enable();

#define atomic_load_and_update(var, old, new, udpate) \
do { \
    old = var; \
    new = udpate; \
} while(cmpxchg(&var, old, new) != old);

void dbg_printf(char *fmt, ...);

