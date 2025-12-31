/* exec/main.c */
#include "server.h"
#include "parse_emergency.h"
#include "parse_rescuers.h"
#include "parse_env.h"
#include "scheduler.h"
#include <string.h>
#include <signal.h>
#include <unistd.h> // per access()
#include "time.h"

// Definizione Variabile Globale
struct emergencyServer server;

// 1. Funzioni Helper (definite PRIMA del main)

void sigHandler(int sig) {
    (void)sig;
    server.shutdown = 1; 
}

void cleanupServer(void) {
    serverLog(LL_INFO, "Cleaning up resources...");
    if (server.pool) pool_destroy(server.pool);
    if (server.mq != (mqd_t)-1) {
        mq_close(server.mq);
        if (server.env_config.queue_name)
            mq_unlink(server.env_config.queue_name); 
    }
    // free(server.twins); // Opzionale
}

void initServer(void) {
    // Inizializza Mutex
    mtx_init(&server.twins_mtx, mtx_plain);
    mtx_init(&server.active_mtx, mtx_plain);

    // Inizializza Array Emergenze Attive
    server.active_cap = MAX_ACTIVE_CAP; 
    server.active_count = 0;
    server.active_emergencies = malloc(sizeof(emergency_t*) * server.active_cap);
    
    
    server.shutdown = 0;
    server.mq = (mqd_t)-1; // Importante per evitare close su handle invalido
}

void loadServerConfig(const char *conf_dir) {
    char filepath[512];
    
    serverLog(LL_INFO, "Loading configuration from directory: %s", conf_dir);

    // -- ENV --
    snprintf(filepath, sizeof(filepath), "%s/env.conf", conf_dir);
    server.env_config = parse_env_config(filepath);
    if (!server.env_config.queue_name) {
        serverLog(LL_ERR, "Fatal: 'queue' missing in %s", filepath);
        exit(1);
    }

    // -- RESCUERS --
    snprintf(filepath, sizeof(filepath), "%s/rescuers.conf", conf_dir);
    rescuers_data_t r_data = parse_rescuers_config(filepath);
    if (r_data.twin_count == 0) {
        serverLog(LL_ERR, "Fatal: No rescuers in %s", filepath);
        exit(1);
    }
    server.twins = r_data.twins;
    server.twins_count = r_data.twin_count;
    server.rescuer_types = r_data.types;
    server.rescuer_types_count = r_data.type_count;

    // -- EMERGENCY TYPES --
    snprintf(filepath, sizeof(filepath), "%s/emergency_types.conf", conf_dir);
    server.em_data = parse_emergency_types_config(filepath, server.rescuer_types, server.rescuer_types_count);
    if (server.em_data.count == 0) {
        serverLog(LL_ERR, "Fatal: No emergency types in %s", filepath);
        exit(1);
    }
    
    serverLog(LL_INFO, "Config OK: %d rescuers, %d types emergencies.", server.twins_count, server.em_data.count);
}

// 2. Main
int main(int argc, char **argv) {
    init_logger("emergenza.log");
    // A. INIT (FONDAMENTALE chiamarlo per primo)
    initServer();

    // B. Segnali
    struct sigaction sa;
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // C. Configurazione (Default a "conf" se non specificato)
    const char *conf_path = (argc > 1) ? argv[1] : "conf";
    loadServerConfig(conf_path);

    // D. Avvio Thread Pool
    server.pool = pool_create(4); // 4 thread worker

    // E. Avvio Listener Coda
    // Apre la coda qui o dentro il listener, ma assicurati che env_config sia carico
    struct mq_attr attr = { .mq_maxmsg = 10, .mq_msgsize = sizeof(emergency_request_t) };
    server.mq = mq_open(server.env_config.queue_name, O_CREAT | O_RDWR, 0666, &attr);
    if (server.mq == (mqd_t)-1) {
        serverLog(LL_ERR, "Failed to open MQ: %s", server.env_config.queue_name);
        perror("mq_open");
        exit(1);
    }

    thrd_t listener_thread;
    if (thrd_create(&listener_thread, (thrd_start_t)acceptEmergencies, NULL) != thrd_success) {
        serverLog(LL_ERR, "Failed to create listener thread");
        exit(1);
    }

    //F. Loop principale
    struct timespec loop_delay;
    loop_delay.tv_sec = 0;             // 0 secondi
    loop_delay.tv_nsec = 100000000;    // 100.000.000 nanosecondi = 100ms
    serverLog(LL_INFO, "Server running. Press Ctrl+C to stop.");

    while(!server.shutdown) {
        //Manutenzione(Aging, Timeout)
        serverCron();
        //controlla le emergenze WAITING e assegna i soccorritori
        assignResources();
        
        nanosleep(&loop_delay, NULL);
    }

    serverLog(LL_WARN, "Shutdown signal received.");
    
    // G. Cleanup
    mq_close(server.mq); // Chiudi la coda per sbloccare il listener (o usa pthread_kill se necessario)
    // thrd_join(listener_thread, NULL); // Spesso il listener è bloccato su mq_receive, meglio lasciar morire
    cleanupServer();
    
    return 0;
}