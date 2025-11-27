/* exec/emergency.c */
#include "server.h"
#include "utils.h" 
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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

/* --- LOGICA CORE: GESTIONE CICLO DI VITA --- */
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