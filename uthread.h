#ifndef _uthread_h_
#define _uthread_h_

#include <stdio.h>
#include <stdint.h>

typedef void *(*uthread_func)(void *);

uint64_t uthread_create(uint64_t *id, uthread_func func, void* arg, char* name);
uint64_t uthread_self(void);
int uthread_join(uint64_t id, void* *statusptr);
void uthread_exit(void *status);

void wrapper_init();
void* uthread_malloc(size_t size);
void uthread_free(void* ptr);
void uthread_printf(const char* fmt, ...);

#endif
