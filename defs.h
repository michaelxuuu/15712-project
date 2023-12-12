#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include <sys/time.h>
#include <sys/mman.h>
#include <sys/syscall.h>

// Order matters here
typedef void *(*uthread_func)(void *);
#include "spinlock.h"
#include "sleeplock.h"
#include "con.h"
#include "tid.h"
#include "tcb.h"
#include "core.h"
#include "runtime.h"

struct tcb;
struct tid;
struct core;
struct context;
struct spinlock;
struct sleeplock;

enum state {
    RUNNING,
    RUNNABLE,
    SLEEPING,
    JOINABLE,
    JOINED
};

#define PAGELEN sysconf(_SC_PAGESIZE)
#define STACKLEN (PAGELEN + PAGELEN)

#define atomic_load_and_update(var, old, new, udpate) \
    do { \
        old = var; \
        new = udpate; \
    } while(cmpxchg(&var, old, new) != old);

// runtime.c
extern struct runtime g;

// core.c (pthread local)
extern __thread struct core* mycore;

// spinlock.c
// Note this function also disable signals on the softcore on which it's called.
void acquire(struct spinlock*);
void release(struct spinlock*);
void spinlock_init(struct spinlock *lk, char *name);

// sleeplock.c
void sleeplock_aquire(struct sleeplock*);
void sleeplock_release(struct sleeplock*);
void sleeplock_init(struct sleeplock*, char *name);

// asm.S
#ifdef __llvm__
int xchg(void* p, int new);
int cmpxchg(void* p, int old, int new);
void swtch(struct context**, struct context*);
#elif defined(__GNUC__)
#define xchg _xchg
#define cmpxchg _cmpxchg
#define swtch _swtch
int _xchg(void* p, int new);
int _cmpxchg(void* p, int old, int new);
void _swtch(struct context**, struct context*);
#else
#error "Unsupported compiler"
#endif

// thread.c
void yield();

// sig.c
void sig_init();
void sig_enable();
void sig_disable();

// timer.c
void timer_init();

// wrapper.c
void wrapper_init();
void* uthread_malloc(size_t size);
void uthread_free(void* ptr);
void uthread_printf(const char* fmt, ...);

// debug.c
void debug_printf(char *fmt, ...);

