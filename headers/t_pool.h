#ifndef T_POOL_H
#define T_POOL_H

#include <threads.h>
#include <stdbool.h>
#include "macro.h"

typedef struct thread_pool thrd_pool_t;

typedef struct{
    int (*function)(void *); //funzione da eseguire
    void *arg; //parametro
}task_t;

thrd_pool_t *pool_create(int max_threads);
bool pool_submit(thrd_pool_t *pool, int (*function)(void *), void *arg);
void pool_destroy(thrd_pool_t *pool);

#endif