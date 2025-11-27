#include <stdlib.h>
#include <stdatomic.h>
#include "t_pool.h"

struct thread_pool{
    thrd_t *threads; //array di thread
    task_t task_queue[TASK_QUEUE_SIZE]; //coda circolare di task
    atomic_int head, tail, count; //gestiscono la coda come FIFO
    int max_threads; //# max di thrad nel pool
    bool shutdown; //flag per fermare i thread
    mtx_t lock; //mutex
    cnd_t has_task; //presenza di un task
};

static int worker(void *arg){
    thrd_pool_t *pool = (thrd_pool_t *)arg;
    while(true){
        mtx_lock(&pool->lock);

        //se la coda è vuota e non siamo in shutdown, sospende
        while(pool->count == 0 && !pool->shutdown)
            cnd_wait(&pool->has_task, &pool->lock);
        
        //se shitdown è vero, esce e termina il thread
        if(pool->shutdown){
            mtx_unlock(&pool->lock);
            break;
        }
        //estrazione e aggiornamento della coda
        task_t task = pool->task_queue[pool->head];
        pool->head = (pool->head +1) % TASK_QUEUE_SIZE;
        pool->count--;

        mtx_unlock(&pool->lock);
        task.function(task.arg); //esegue il task
    }
    return 0;
}

//creazione del pool
thrd_pool_t *pool_create(int max_threads){
    thrd_pool_t *pool;
    SAFE_MALLOC(pool, sizeof(thrd_pool_t));

    SAFE_MALLOC(pool->threads, sizeof(thrd_t)*max_threads);
    pool->max_threads = max_threads;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = false; 

    mtx_init(&pool->lock, mtx_plain);
    cnd_init(&pool->has_task);

    for(int i = 0; i < max_threads; i++)
        thrd_create(&pool->threads[i], worker, pool);
    return pool;
}

bool pool_submit(thrd_pool_t *pool, int (*function)(void *), void *arg) {
    mtx_lock(&pool->lock);
    //se la coda è piena
    if (pool->count == TASK_QUEUE_SIZE) {
        mtx_unlock(&pool->lock);
        return false;
    }

    //altrimenti aggiungiamo in tail e aggiorniamo
    task_t temp = { .function = function, .arg = arg }; //istanza temporanea
    pool->task_queue[pool->tail] = temp; //aggiunta in coda
    pool->tail = (pool->tail + 1) % TASK_QUEUE_SIZE;
    pool->count++;
    cnd_signal(&pool->has_task); //risvegliare un thread worker
    mtx_unlock(&pool->lock); //rilascio del lock
    return true;
}


//Distruzione del pool
void pool_destroy(thrd_pool_t *pool) {
    mtx_lock(&pool->lock);
    pool->shutdown = true;
    cnd_broadcast(&pool->has_task); //risveglio dei trhead
    mtx_unlock(&pool->lock);

    for (int i = 0; i < pool->max_threads; i++)
        thrd_join(pool->threads[i], NULL); //attesa che tutit i thread terminano

    //Liberazione di memoria e risorse
    mtx_destroy(&pool->lock);
    cnd_destroy(&pool->has_task);
    free(pool->threads);
    free(pool);
}