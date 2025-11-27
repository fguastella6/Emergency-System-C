#ifndef MACRO_H
#define MACRO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "logger.h"
//controllo memoria e file
#define SAFE_MALLOC(p, size) do { \
    p = malloc(size); \
    if (!p) { perror("malloc"); exit(EXIT_FAILURE); } \
} while (0)

#define SAFE_REALLOC(p, size) do { \
    void *tmp = realloc(p, size); \
    if (!tmp) { perror("realloc"); exit(EXIT_FAILURE); } \
    p = tmp; \
} while(0)

#define SAFE_FOPEN_1(fp, path, mode) \
    do { fp = fopen(path, mode); if (!fp) { perror("fopen"); exit(EXIT_FAILURE); } } while (0)

#define SAFE_FOPEN(fp, path, mode, log_id) \
    do { \
        fp = fopen(path, mode); \
        if (!fp) { \
            log_parsing_event(log_id, "ERRORE", "Impossibile aprire il file"); \
            perror("fopen"); exit(EXIT_FAILURE); \
        } \
        log_parsing_event(log_id, "APERTURA", "Inizio parsing"); \
    } while (0)

#define SAFE_MQ_OPEN(mq, name, flags, mode, attr) do{ \
    mq = mq_open((name), (flags), (mode), (attr)); \
    if (mq == (mqd_t)-1){ perror ("mq_open"); exit(EXIT_FAILURE);} \
}while(0)

#define SAFE_MQ_RECV(rc, mq, buf, buflen, prio) do{ \
    rc = mq_receive((mq), (char*)(buf), (buflen), (prio)); \
    if (rc < 0){ perror ("mq_recive");} \
}while(0)

#define DEFAULT_LOG_PATH "log/log.txt"
#define N_THREAD 100
#define MAX_LINE 256
#define MAX_RESCUERS_PER_TYPE 10
#define MAX_KEY_LEN 32
#define MAX_VAL_LEN 128
#define MAX_EMERGENZE_ATTIVE 128
#define MSG_LEN 128
#define TASK_QUEUE_SIZE 128
#define MAX_ACTIVE_CAP 100

//Ccostanti per aging
#define AGING_INTERVAL 2          // ogni 2 secondi
#define PROMOTE_TO_1_SEC 5     // dopo 30s → 1

#endif

