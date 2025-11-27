#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers_pars/parse_env.h"
#include "macro.h"
#include "utils.h"
#include "logger.h"

env_config_t parse_env_config(const char *filename){
    FILE *fp;
    SAFE_FOPEN(fp, filename, "r", filename);

    env_config_t config = {0};
    char line[MAX_LINE];
    
    while(fgets(line, sizeof(line), fp)){

        log_parsing_event(filename, "RIGA_LETTA", line);
        rimuovi_spazi(line);

        char key[MAX_KEY_LEN], value[MAX_VAL_LEN];

        if(sscanf(line, "%63[^=]=%127s", key, value) == 2){
            if(strcmp(key, "queue") == 0){
                char name_q[MAX_VAL_LEN +4];
                snprintf(name_q, sizeof(name_q), "/%s",value);
                config.queue_name = my_strdup(name_q);
                log_parsing_event(filename, "PARAMETRO", "queue");

            }else if (strcmp(key, "height") == 0){
                config.height = atoi(value);
                log_parsing_event(filename, "PARAMETRO", "height");

            }else if (strcmp(key, "width") == 0){
                config.width = atoi(value);
                log_parsing_event(filename, "PARAMETRO", "width");

            }else{
                log_parsing_event(filename, "ERRORE_FORMATO", line);
            }
        }else{
            log_parsing_event(filename, "ERRORE_FORMATO", line);
        }
    }
    fclose(fp);

    if (!is_nonempty_string(config.queue_name) || 
        !is_positive(config.height) || !is_positive(config.width)) {
        log_parsing_event(filename, "ERRORE", "Parametri env non validi");
        config.queue_name = NULL;
    }else{
        log_parsing_event(filename, "FINE", "Parsing completato (env)");
    }

    return config;
}