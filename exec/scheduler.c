#include <time.h>
#include <stdio.h>
#include "struct.h"
#include "server.h"

void serverCron(void) {
    time_t now = time(NULL);

    mtx_lock(&server.active_mtx);

    for (int i = 0; i < server.active_count; i++) {
        emergency_t *em = server.active_emergencies[i];

        // Se è in attesa e la priorità non è già massima (assumiamo 2 come max)
        if (em->status == WAITING && em->current_priority < 2) {
            
            double elapsed = difftime(now, em->request_timestamp);

            if (elapsed > THRESHOLD_AGING) {
                em->current_priority++;
                
                // Aggiorniamo il timestamp per evitare che scatti di nuovo subito dopo
                em->request_timestamp = now; 

                serverLog(LL_WARN, "[AGING] Emergency %s priority increased to %d (waited %.0fs)", 
                          em->id, em->current_priority, elapsed);
                
                // Sottomettiamo di nuovo il task al pool.
                // Se il pool è pieno, pazienza, riproveremo al prossimo giro di cron.
                pool_submit(server.pool, processEmergency, em);
            }
        }
    }

    mtx_unlock(&server.active_mtx);
}

/* Aggiunge un'emergenza alla lista attiva */
void registerEmergency(emergency_t *em) {
    mtx_lock(&server.active_mtx);
    
    // Espansione dinamica se serve (stile vector C++)
    if (server.active_count >= server.active_cap) {
        int new_cap = server.active_cap * 2;
        emergency_t **tmp = realloc(server.active_emergencies, new_cap * sizeof(emergency_t*));
        if (tmp) {
            server.active_emergencies = tmp;
            server.active_cap = new_cap;
        }
    }

    if (server.active_count < server.active_cap) {
        server.active_emergencies[server.active_count++] = em;
    }
    
    mtx_unlock(&server.active_mtx);
}

/* Rimuove un'emergenza dalla lista attiva */
void unregisterEmergency(emergency_t *em) {
    mtx_lock(&server.active_mtx);
    
    for (int i = 0; i < server.active_count; i++) {
        if (server.active_emergencies[i] == em) {
            // Trovato! Per rimuoverlo velocemente senza buchi:
            // Spostiamo l'ultimo elemento al posto di questo (Swap-with-last)
            // Tanto l'ordine non conta per l'aging
            server.active_emergencies[i] = server.active_emergencies[server.active_count - 1];
            server.active_count--;
            break;
        }
    }
    
    mtx_unlock(&server.active_mtx);
}