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

// Trova i K soccorritori più vicini di un certo tipo
// Ritorna: numero di soccorritori trovati e idonei.
// Riempie l'array `results` con gli indici nell'array globale `server.twins`.
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

/* emergency.c - Funzione processEmergency aggiornata */

int processEmergency(void *arg) {
    emergency_t *em = (emergency_t *)arg;
    int success = 0;
    
    // Array per ricordarci chi abbiamo preso (per usarlo nelle fasi successive)
    // Usiamo un array di puntatori diretti ai twin per comodità
    rescuer_digital_twin_t *my_rescuers[50]; 
    int my_rescuers_count = 0;

    // ---------------------------------------------------------
    // FASE 1: PRENOTAZIONE (IDLE -> EN_ROUTE)
    // ---------------------------------------------------------
    mtx_lock(&server.twins_mtx);

    // ... (Qui c'è la logica di ricerca find_nearest_rescuers che abbiamo scritto prima) ...
    // Supponiamo tu abbia riempito 'booked_indices' e 'requirements_met' sia true
    
    /* ESEMPIO DI INTEGRAZIONE LOGICA DI RICERCA */
    int booked_indices[50];
    int booked_count = 0;
    int requirements_met = 1;

    for (int i = 0; i < em->type.rescuers_req_number; i++) {
        rescuer_request_t *req = &em->type.rescuers[i];
        int type_indices[req->required_count];
        
        if (find_nearest_rescuers(req->type->rescuer_type_name, req->required_count, em->x, em->y, type_indices) == req->required_count) {
            for(int k=0; k<req->required_count; k++) 
                booked_indices[booked_count++] = type_indices[k];
        } else {
            requirements_met = 0;
            break;
        }
    }

    if (requirements_met) {
        // COMMIT
        for (int i = 0; i < booked_count; i++) {
            rescuer_digital_twin_t *dt = &server.twins[booked_indices[i]];
            
            // Cambio Stato
            dt->status = EN_ROUTE_TO_SCENE;
            dt->owner = em;
            
            // Salviamo il puntatore per dopo (così non dobbiamo ricercarli per indice)
            my_rescuers[my_rescuers_count++] = dt;

            // LOG IDLE -> EN_ROUTE
            serverLog(LL_INFO, "[RESCUER] %s_%d: Assigned to %s. Status IDLE -> EN_ROUTE.", 
                      dt->rescuer->rescuer_type_name, dt->id, em->id);
        }
        em->status = IN_PROGRESS; // o ASSIGNED
        success = 1;
    } else {
        // ROLLBACK
        em->status = WAITING;
        serverLog(LL_DEBUG, "Emergency %s: Resources busy, retry later.", em->id);
    }
    mtx_unlock(&server.twins_mtx);

    if (!success) return 0; // Uscita anticipata

    // ---------------------------------------------------------
    // FASE 2: VIAGGIO -> ARRIVO (EN_ROUTE -> ON_SCENE)
    // ---------------------------------------------------------
    
    // Qui simuli il tempo di viaggio (es. calcoli distanza max)
    // sleep(2); 

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
    // sleep(em->type.rescuers[0].time_to_manage); 
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
        
        // LOG ON_SCENE -> RETURNING
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
    // sleep(2);
    nanosleep(&work_time, NULL);

    mtx_lock(&server.twins_mtx);
    for (int i = 0; i < my_rescuers_count; i++) {
        rescuer_digital_twin_t *dt = my_rescuers[i];
        
        // Controllo di sicurezza: siamo ancora noi i proprietari?
        if (dt->owner == em) {
            // Cambio Stato
            dt->status = IDLE;
            dt->owner = NULL;
            
            // Ripristino coordinate base (fondamentale da specifiche!)
            // Assumiamo che dt->rescuer->x/y siano le coordinate base statiche lette dal config
            dt->x = dt->rescuer->x; 
            dt->y = dt->rescuer->y;

            // LOG RETURNING -> IDLE
            serverLog(LL_INFO, "[RESCUER] %s_%d: Back at base (%d, %d). Status RETURNING -> IDLE.", 
                      dt->rescuer->rescuer_type_name, dt->id, dt->x, dt->y);
        }
    }
    mtx_unlock(&server.twins_mtx);

    // Cleanup memoria emergenza
    freeEmergency(em);

    return 0;
}
/* --- LOGICA CORE: GESTIONE CICLO DI VITA --- 
int processEmergency(void *arg) {
    emergency_t *em = (emergency_t*)arg;
    
    // Array per tracciare i soccorritori selezionati
    rescuer_digital_twin_t *chosen[MAX_RESCUERS_PER_TYPE * 5]; 
    int chosen_count = 0;
    int success = 0;

    // --- FASE 1: SELEZIONE ATOMICA (CRITICA) ---
    mtx_lock(&server.twins_mtx);

    // Verifichiamo se riusciamo a soddisfare TUTTI i requisiti
    int requirements_met = 1;

    // Iteriamo su ogni richiesta dell'emergenza (es. "Serve 1 Ambulanza", "Serve 2 Polizia")
    for (int r = 0; r < em->type.rescuers_req_number; r++) {
        rescuer_request_t *req = &em->type.rescuers[r];
        int needed = req->required_count;
        int found = 0;

        // Scansioniamo tutti i gemelli disponibili nel sistema
        for (int t = 0; t < server.twins_count; t++) {
            rescuer_digital_twin_t *dt = &server.twins[t];

            // 1. Il tipo corrisponde?
            if (strcmp(dt->rescuer->rescuer_type_name, req->type->rescuer_type_name) != 0) 
                continue;

            // 2. È libero (IDLE)?
            if (dt->status != IDLE) 
                continue;

            // 3. È già stato preso in questo giro (per evitare doppioni)?
            int already_picked = 0;
            for(int k=0; k<chosen_count; k++) {
                if(chosen[k] == dt) { already_picked = 1; break; }
            }
            if (already_picked) continue;

            // SELEZIONATO!
            chosen[chosen_count++] = dt;
            found++;

            if (found >= needed) break; // Requisito soddisfatto, passo al prossimo tipo
        }

        if (found < needed) {
            requirements_met = 0; // Manca qualcosa, abortiamo tutto
            break;
        }
    }

    if (requirements_met) {
        // SUCCESSO: Prenotiamo le risorse
        serverLog(LL_INFO, "Emergency %s: STARTING. Assigned %d rescuers.", em->id, chosen_count);
        
        for(int k=0; k<chosen_count; k++) {
            chosen[k]->status = EN_ROUTE_TO_SCENE;
            chosen[k]->owner = em;
        }
        em->status = IN_PROGRESS; // O EN_ROUTE
        success = 1;
    } else {
        // FALLIMENTO: Non blocchiamo nessuno (Deadlock prevention)
        // Reset count per sicurezza (anche se è locale)
        serverLog(LL_DEBUG, "Emergency %s: Waiting for resources...", em->id);
        em->status = WAITING;
    }

    mtx_unlock(&server.twins_mtx);

    // --- FASE 2: SIMULAZIONE TEMPORALE (NON BLOCCANTE) ---
    if (success) {
        // A. VIAGGIO VERSO SCENA (EN_ROUTE)
        int max_travel_time = 0;
        for(int k=0; k<chosen_count; k++) {
            int t = eta_secs(chosen[k], em->x, em->y);
            if (t > max_travel_time) max_travel_time = t;
        }
        
        // Simuliamo il viaggio
        sleep_2(em, max_travel_time);
        
        // B. ARRIVO SULLA SCENA (ON_SCENE)
        mtx_lock(&server.twins_mtx);
        for(int k=0; k<chosen_count; k++) chosen[k]->status = ON_SCENE;
        serverLog(LL_INFO, "Emergency %s: Units ON SCENE. Working...", em->id);
        mtx_unlock(&server.twins_mtx);

        // Simuliamo il lavoro (es. 5 secondi fissi o definiti nel config)
        sleep_2(em, 5); 

        // C. RIENTRO ALLA BASE (RETURNING)
        mtx_lock(&server.twins_mtx);
        for(int k=0; k<chosen_count; k++) chosen[k]->status = RETURNING_TO_BASE;
        mtx_unlock(&server.twins_mtx);
        
        // Simuliamo il rientro (stesso tempo dell'andata per semplicità)
        sleep_2(em, max_travel_time);

        // D. FINE (Rilascio Risorse)
        mtx_lock(&server.twins_mtx);
        for(int k=0; k<chosen_count; k++) {
            chosen[k]->status = IDLE;
            chosen[k]->owner = NULL;
            // Aggiorna posizione soccorritore (ora è tornato alla base originale x,y)
            // Se volessi simulare che restano sul posto, aggiorneresti qui.
            // Ma per il progetto, "Returning to base" implica che tornano a casa.
        }
        em->status = COMPLETED;
        mtx_unlock(&server.twins_mtx);

        serverLog(LL_INFO, "Emergency %s: COMPLETED & CLOSED.", em->id);
    }

    // --- FASE 3: CLEANUP ---
    // Se completata o cancellata -> Rimuovi dalla memoria
    // Se WAITING -> Lascia lì, ci penserà serverCron a riprovare
    if (em->status != WAITING) {
        unregisterEmergency(em);
        freeEmergency(em);
    }
    
    return 0;
}
    */