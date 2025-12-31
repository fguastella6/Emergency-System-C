#ifndef SERVER_H
#define SERVER_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <stdatomic.h>
#include <mqueue.h>
#include <stdarg.h>

#include "struct.h" 
#include "parse_env.h"
#include "parse_rescuers.h"
#include "parse_emergency.h"
#include "t_pool.h"

/* Costanti */
#define LL_DEBUG 0
#define LL_INFO  1
#define LL_WARN  2
#define LL_ERR   3

#define THRESHOLD_AGING  10
#define MAX_ACTIVE_CAP   100 // Capacità iniziale array emergenze

/* --- GOD STRUCT --- */
struct emergencyServer {
    // 1. Configurazione & Risorse Statiche
    env_config_t env_config; 
    
    rescuer_digital_twin_t *twins; 
    int twins_count;
    mtx_t twins_mtx; // Lock per i soccorritori
    
    rescuer_type_t *rescuer_types;
    int rescuer_types_count;
    
    emergency_data_t em_data;

    // 2. Runtime: Emergenze Attive (Per Scheduler/Aging) <--- ECCOLI!
    emergency_t **active_emergencies; 
    int active_count;
    int active_cap;      
    mtx_t active_mtx;    // Lock per la lista emergenze

    thrd_pool_t *pool;
    mqd_t mq;
    atomic_int shutdown;
};

extern struct emergencyServer server;

/* Prototipi */
void initServer(void);
void loadServerConfig(const char *conf_dir);
void serverLog(int level, const char *fmt, ...);

int compare_dist(const void *a, const void *b);
int find_nearest_rescuers(const char *type_name, int count_needed, int em_x, int em_y, int *results_indices);
// Gestione Emergenze
emergency_t *createEmergencyFromRequest(emergency_request_t *req);
void freeEmergency(emergency_t *em);
int processEmergency(void *arg);

int acceptEmergencies(void *arg);

#endif
