#include "server.h"
#include "scheduler.h"
#include <errno.h>
#include <time.h>
#include "string.h"

/* Funzione worker che ascolta la coda */
int acceptEmergencies(void *arg) {
    (void)arg;
    // Buffer locale
    char msg_buf[1024]; // Dimensione sicura
    unsigned int prio;
    
    // Attributi coda (recuperati da server.config se necessario)
    // struct mq_attr attr;
    // mq_getattr(server.mq, &attr);

    serverLog(LL_INFO, "Listening for emergencies on queue...");

    while(!server.shutdown) {
        // Ricezione bloccante (o con timeout breve per controllare shutdown)
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1; // 1 secondo timeout per controllare shutdown

        ssize_t bytes = mq_timedreceive(server.mq, msg_buf, sizeof(msg_buf), &prio, &ts);

        if (bytes < 0) {
            if (errno == ETIMEDOUT || errno == EINTR) continue;
            serverLog(LL_WARN, "MQ receive error: %s", strerror(errno));
            continue;
        }

        if (bytes < (ssize_t)sizeof(emergency_request_t)) {
             serverLog(LL_WARN, "NETWORK: Received packet too small/corrupted");
             continue;
        }

        // Parsing rapido (assumendo casting diretto o memcpy se la struct è POD)
        emergency_request_t *req = (emergency_request_t*)msg_buf;
        
        /* Assicuriamo che le stringhe siano terminate (sicurezza) */
        req->emergency_name[sizeof(req->emergency_name)-1] = '\0';
        
        // Creazione dell'oggetto emergenza (allocazione dinamica)
        emergency_t *em = createEmergencyFromRequest(req); // Da implementare in emergency.c
        if (!em) {
            serverLog(LL_ERR, "Failed to create emergency object");
            continue;
        }
        registerEmergency(em);
            

        serverLog(LL_INFO, "New Request: %s at (%d, %d)", em->type.emergency_desc, em->x, em->y);
    }
    return 0;
}

