#include "defs.h"

static struct sleeplock malloc_lk;
static struct sleeplock printf_lk;

void
wrapper_init() {
    sleeplock_init(&malloc_lk, "malloc.lk");
    sleeplock_init(&printf_lk, "printf.lk");
}

void*
uthread_malloc(size_t size) {
    void* ptr;
    sleeplock_aquire(&malloc_lk);
    ptr = malloc(size);
    sleeplock_release(&malloc_lk);
    return ptr;
}

void
uthread_free(void* ptr) {
    sleeplock_aquire(&malloc_lk);
    free(ptr);
    sleeplock_release(&malloc_lk);
}

void
uthread_printf(const char* fmt, ...) {
    sleeplock_aquire(&printf_lk);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    sleeplock_release(&printf_lk);
}
