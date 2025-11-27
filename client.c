#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "server.h"
#include "logger.h"
#include "macro.h"
#include "struct.h"
#include "parse_env.h"
#include "utils.h"

typedef struct {
    mqd_t mq;             // (mqd_t)-1 se non aperta
    env_config_t config;  // contiene queue_name 
    int logger_init;      // 1 se init_logger effettuato
} client_res_t;

static void res_init(client_res_t *r) {
    r->mq = (mqd_t)-1;
    r->logger_init = 0;
}

static void res_cleanup(client_res_t *r) {
    if (r->mq != (mqd_t)-1) {
        mq_close(r->mq);
        r->mq = (mqd_t)-1;
    }
    if (r->logger_init) {
        close_logger();
        r->logger_init = 0;
    }
}

static int invia_emergenza(mqd_t mq, const char *nome, int x, int y, int delay) {
    emergency_request_t req;
    // Copia sicura del nome (EMERGENCY_NAME_LENGTH include lo '\0')
    snprintf(req.emergency_name, EMERGENCY_NAME_LENGTH, "%s", nome);

    req.x = x;
    req.y = y;
    req.timestamp = time(NULL) + delay;
    if (mq_send(mq, (const char*)&req, sizeof(req), 0) == -1) {
        perror("mq_send");
        return -1;
    }
    printf("Emergenza '%s' inviata per posizione (%d,%d) con delay %d sec\n",
           req.emergency_name, x, y, delay);
    return 0;
}

static int invia_da_file(mqd_t mq, const char *filename) {
    FILE *fp;
    SAFE_FOPEN(fp, filename, "r", filename);
    // Se la tua SAFE_FOPEN fa exit() non si arriva qui; se invece restituisce NULL:
    if (!fp) {
        perror("fopen");
        return -1;
    }

    char line[MAX_LINE];
    int rc = 0;
    while (fgets(line, sizeof(line), fp)) {
        char nome[64];
        int x, y, delay;

        // Limite per 'nome' per evitare overflow
        if (sscanf(line, "%63s %d %d %d", nome, &x, &y, &delay) == 4) {
            if (is_valid_coordinate(x, y) && is_valid_delay(delay) && is_nonempty_string(nome)) {
                if (invia_emergenza(mq, nome, x, y, delay) != 0) {
                    rc = -1; // fallito un invio: segnalo ma continuo
                }
                sleep(1); // piccola pausa per evitare congestione
            } else {
                fprintf(stderr, "Valori non validi nella riga: %s", line);
                rc = -1;
            }
        } else {
            fprintf(stderr, "Formato riga non valido: %s", line);
            rc = -1;
        }
    }
    fclose(fp);
    return rc;
}

int main(int argc, char *argv[]) {
    client_res_t res;
    res_init(&res);

    init_logger("emergenza.log");
    serverLog(1, "CLIENT STARTED: PID %d", getpid());

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <nome_emergenza> <x> <y> <ritardo_sec>\n", argv[0]);
        fprintf(stderr, "   oppure: %s -f <file_input>\n", argv[0]);
        res_cleanup(&res);
        return EXIT_FAILURE;
    }

    // parsing env
    res.config = parse_env_config("conf/env.conf");
    if (!res.config.queue_name) {
        fprintf(stderr, "Errore: parsing env.conf fallito (queue_name mancante)\n");
        res_cleanup(&res);
        return EXIT_FAILURE;
    }

    // apertura coda POSIX
    res.mq = mq_open(res.config.queue_name, O_WRONLY);
    if (res.mq == (mqd_t)-1) {
        perror("mq_open");
        res_cleanup(&res);
        return EXIT_FAILURE;
    }

    int status = 0;

    if (strcmp(argv[1], "-f") == 0 && argc == 3) {
        status = invia_da_file(res.mq, argv[2]);
    } else if (argc == 5) {
        const char *nome = argv[1];
        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        int delay = atoi(argv[4]);

        if (!is_nonempty_string(nome) || !is_valid_coordinate(x, y) || !is_valid_delay(delay)) {
            fprintf(stderr, "Argomenti non validi\n");
            status = -1;
        } else {
            status = invia_emergenza(res.mq, nome, x, y, delay);
        }
    } else {
        fprintf(stderr, "Argomenti non validi\n");
        status = -1;
    }

    res_cleanup(&res);
    close_logger();
    return (status == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    
}
