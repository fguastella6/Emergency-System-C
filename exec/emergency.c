/* exec/emergency.c */
#include "server.h"
#include "scheduler.h"
#include "utils.h" 
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// Funzione di comparazione per qsort (ordina per distanza crescente)
typedef struct {
    int index;
    int distance;
} dist_info_t;

int compare_dist(const void *a, const void *b) {
    return ((dist_info_t*)a)->distance - ((dist_info_t*)b)->distance;
}

/*---------- find_nearest_rescuiers
* Trova i K soccorritori più vicini di un certo tipo
* Ritorna: numero di soccorritori trovati e idonei.
* Riempie l'array `results` con gli indici nell'array globale `server.twins`.
*/ 
int find_nearest_rescuers(const char *type_name, int count_needed, int em_x, int em_y, int *results_indices) {
    
    // 1. Creiamo una lista temporanea di candidati
    // NOTA: Questa funzione va chiamata SOLO quando si ha già il lock su server.twins_mtx
    dist_info_t *candidates = malloc(sizeof(dist_info_t) * server.twins_count);
    int candidates_count = 0;

    for (int i = 0; i < server.twins_count; i++) {
        rescuer_digital_twin_t *dt = &server.twins[i];
        
        if (dt->status == IDLE && strcmp(dt->rescuer->rescuer_type_name, type_name) == 0) {
            candidates[candidates_count].index = i;
            candidates[candidates_count].distance = distanza_manhattan(dt->x, dt->y, em_x, em_y);
            candidates_count++;
        }
    }

    // 2. Se non ne abbiamo abbastanza, ci fermiamo subito
    if (candidates_count < count_needed) {
        free(candidates);
        return 0; // Fallimento
    }

    // 3. Ordiniamo per distanza
    qsort(candidates, candidates_count, sizeof(dist_info_t), compare_dist);

    // 4. Prendiamo i primi K
    for (int i = 0; i < count_needed; i++) {
        results_indices[i] = candidates[i].index;
    }
    
    free(candidates);
    return count_needed;
}

/* Helper: Cerca il tipo di emergenza nel database */
static emergency_type_t *findEmergencyType(const char *name) {
    for (int i = 0; i < server.em_data.count; i++) {
        if (strcmp(server.em_data.types[i].emergency_desc, name) == 0) {
            return &server.em_data.types[i];
        }
    }
    return NULL;
}

/* Factory: Crea l'emergenza dalla richiesta raw */
emergency_t *createEmergencyFromRequest(emergency_request_t *req) {
    emergency_type_t *type = findEmergencyType(req->emergency_name);
    if (!type) {
        serverLog(LL_WARN, "Unknown emergency type: %s", req->emergency_name);
        return NULL;
    }

    emergency_t *em = calloc(1, sizeof(emergency_t));
    if (!em) return NULL;

    snprintf(em->id, sizeof(em->id), "%ld-%s", req->timestamp, req->emergency_name);
    em->type = *type; 
    em->x = req->x;
    em->y = req->y;
    em->request_timestamp = req->timestamp;
    em->status = WAITING;
    em->current_priority = type->priority;

    return em;
}

void freeEmergency(emergency_t *em) {
    if (em) free(em);
}


/*
 * ------------------------------ processEmergency (Worker Task)
 * Tenta di acquisire le risorse con Mutex unico
 * Se acquisite: cambia stato -> EN_ROUTE -> ON_SCENE -> lavora -> RETURNING.
 * Se fallisce: rimette l'emergenza in WAITING.
 */
int processEmergency(void *arg) {
    emergency_t *em = (emergency_t *)arg;
    int success = 0;
    
    // Array per ricordarci chi abbiamo preso 
    rescuer_digital_twin_t *my_rescuers[50]; 
    int my_rescuers_count = 0;

    // ---------------------------------------------------------
    // FASE 1: PRENOTAZIONE (IDLE -> EN_ROUTE)
    // ---------------------------------------------------------
    mtx_lock(&server.twins_mtx);

    
    /* INTEGRAZIONE LOGICA DI RICERCA (simil algoritmo del banchiere) */
    int booked_indices[50];
    int booked_count = 0;
    int requirements_met = 1;

    for (int i = 0; i < em->type.rescuers_req_number; i++) {
        rescuer_request_t *req = &em->type.rescuers[i];
        int type_indices[req->required_count];
        
        //Interrogo la griglia per trovare i soccorritori liberi più vicini alle coordinate
        if (find_nearest_rescuers(req->type->rescuer_type_name, req->required_count, em->x, em->y, type_indices) == req->required_count) {
            for(int k=0; k<req->required_count; k++) 
                booked_indices[booked_count++] = type_indices[k];
        } else {
            requirements_met = 0;
            break;
        }
    }
    //Se trovo tutti i soccorritori necessari
    if (requirements_met) {
        // COMMIT
        for (int i = 0; i < booked_count; i++) {
            rescuer_digital_twin_t *dt = &server.twins[booked_indices[i]];
            
            // Cambio Stato
            dt->status = EN_ROUTE_TO_SCENE;
            dt->owner = em;
            
            // Salviamo il puntatore per dopo
            my_rescuers[my_rescuers_count++] = dt;

            // LOG IDLE -> EN_ROUTE
            serverLog(LL_INFO, "[RESCUER] %s_%d: Assigned to %s. Status IDLE -> EN_ROUTE.", 
                      dt->rescuer->rescuer_type_name, dt->id, em->id);
        }
        em->status = IN_PROGRESS;
        success = 1;
    } else { //Se ne manca anche solo uno
        // ROLLBACK
        em->status = WAITING;
        serverLog(LL_DEBUG, "Emergency %s: Resources busy, retry later.", em->id);
    }
    mtx_unlock(&server.twins_mtx); //rilascio del lock

    if (!success) return 0; // Uscita anticipata

    // ---------------------------------------------------------
    // FASE 2: VIAGGIO -> ARRIVO (EN_ROUTE -> ON_SCENE)
    // ---------------------------------------------------------
    
    // Tempo di viaggio
    sleep(RESCUER_TRAVEL_TIME); 

    mtx_lock(&server.twins_mtx);
    for (int i = 0; i < my_rescuers_count; i++) {
        rescuer_digital_twin_t *dt = my_rescuers[i];
        
        // Cambio Stato
        dt->status = ON_SCENE;
        
        // Aggiorna posizione (Teletrasporto all'emergenza)
        dt->x = em->x;
        dt->y = em->y;

        // LOG EN_ROUTE -> ON_SCENE
        serverLog(LL_INFO, "[RESCUER] %s_%d: Arrived at scene (%d, %d). Status EN_ROUTE -> ON_SCENE.", 
                  dt->rescuer->rescuer_type_name, dt->id, em->x, em->y);
    }
    mtx_unlock(&server.twins_mtx);

    // ---------------------------------------------------------
    // FASE 3: INTERVENTO
    // ---------------------------------------------------------
    
    serverLog(LL_INFO, "Emergency %s: Intervention in progress...", em->id);
    
    // Simulazione lavoro effettivo
    struct timespec work_time = {15, 0}; // Esempio 2 sec
    nanosleep(&work_time, NULL);


    // ---------------------------------------------------------
    // FASE 4: FINE INTERVENTO (ON_SCENE -> RETURNING)
    // ---------------------------------------------------------
    mtx_lock(&server.twins_mtx);
    for (int i = 0; i < my_rescuers_count; i++) {
        rescuer_digital_twin_t *dt = my_rescuers[i];
        // Cambio Stato
        dt->status = RETURNING_TO_BASE;
        
        serverLog(LL_INFO, "[RESCUER] %s_%d: Job done. Status ON_SCENE -> RETURNING_TO_BASE.", 
                  dt->rescuer->rescuer_type_name, dt->id);
    }
    // Aggiorniamo stato emergenza
    em->status = COMPLETED;
    unregisterEmergency(em); // Togliamo dalla lista active
    mtx_unlock(&server.twins_mtx);

    serverLog(LL_INFO, "Emergency %s: COMPLETED.", em->id);

    // ---------------------------------------------------------
    // FASE 5: RIENTRO (RETURNING -> IDLE)
    // ---------------------------------------------------------
    
    // Simulazione viaggio di ritorno
    nanosleep(&work_time, NULL);

    mtx_lock(&server.twins_mtx);
    for (int i = 0; i < my_rescuers_count; i++) {
        rescuer_digital_twin_t *dt = my_rescuers[i];
        
        // Controllo di sicurezza: siamo ancora noi i proprietari?
        if (dt->owner == em) {
            // Cambio Stato
            dt->status = IDLE;
            dt->owner = NULL;
            
            // Ripristino coordinate base 
            dt->x = dt->rescuer->x; 
            dt->y = dt->rescuer->y;

            // LOG RETURNING -> IDLE
            serverLog(LL_INFO, "[RESCUER] %s_%d: Back at base (%d, %d). Status RETURNING_TO_BASE -> IDLE.", 
                      dt->rescuer->rescuer_type_name, dt->id, dt->x, dt->y);
        }
    }
    mtx_unlock(&server.twins_mtx);

    // Cleanup memoria emergenza
    freeEmergency(em);

    return 0;
}