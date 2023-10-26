#include "defs.h"

struct sleeplock malloc_lk = SLEEPSPINLOCK_INIT_LIST;
struct sleeplock printf_lk = SLEEPSPINLOCK_INIT_LIST;

void*
_malloc(size_t size) {
    void* ptr;
    sleeplock_aquire(&malloc_lk);
    ptr = malloc(size);
    sleeplock_release(&malloc_lk);
    return ptr;
}

void
_free(void* ptr) {
    sleeplock_aquire(&malloc_lk);
    free(ptr);
    sleeplock_release(&malloc_lk);
}

void
_printf(const char* fmt, ...) {
    sleeplock_aquire(&printf_lk);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    sleeplock_release(&printf_lk);
}
