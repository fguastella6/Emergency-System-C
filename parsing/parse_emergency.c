#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers_pars/parse_emergency.h"
#include "macro.h"
#include "utils.h"

static rescuer_type_t *find_rescuer(const char *name, rescuer_type_t *types, int type_count){
    for(int i = 0; i < type_count; i++){
        if(strcmp(name, types[i].rescuer_type_name)== 0)
            return &types[i];
    }
    return NULL;
}

emergency_data_t parse_emergency_types_config(const char * filename , rescuer_type_t *available_types, int type_count){
    FILE *fp;
    SAFE_FOPEN(fp, filename, "r", filename);

    emergency_data_t data = {0};
    int capacity = 10;
    SAFE_MALLOC(data.types, sizeof(emergency_type_t)* capacity);
    char line[MAX_LINE];

    while(fgets(line, sizeof(line), fp)){
        //parsing nome e prioritò
        log_parsing_event(filename, "RIGA_LETTA", line);
        rimuovi_spazi(line);

        char name[64];
        int priority;
        if (sscanf(line, "[ %[^]] ] [ %d] ", name, &priority) != 2 || priority < 0 || priority > 2) {
            log_parsing_event(filename, "ERRORE_FORMATO", line);
            continue;
        }

        char *rescuer_str;
        
        //elenco dei soccorritori
        rescuer_str = strchr(line,']'); //dopo nome
        if(rescuer_str) rescuer_str = strchr(rescuer_str +1,']'); //dopo priorità
        if (!rescuer_str) {
            log_parsing_event(filename, "ERRORE_FORMATO", "Delimitatori mancanti nella riga");
            continue;
        }
        rescuer_str ++;     //inizia la lista dei rescuers
        rescuer_str += strspn(rescuer_str, "\t");

        //creeazione di una nuova emergenza
        emergency_type_t *etype = &data.types[data.count++];
        etype->priority = (short)priority;
        etype->emergency_desc = my_strdup(name);
        SAFE_MALLOC(etype->rescuers, sizeof(rescuer_request_t) * MAX_RESCUERS_PER_TYPE);
        etype->rescuers_req_number = 0;

        //parsing della lista soccorritori
        int error_log = 0;
        char *token = strtok(rescuer_str, ";");
        while(token){
            token += strspn(token, " \t\r\n");
            if(!*token) {
                token = strtok(NULL, ";");
                continue;
            }
            
            
            char rescuer_name[64] = {0};
            int count, time;
            if(sscanf(token, "%[^:]:%d,%d", rescuer_name, &count, &time) == 3 && 
                is_positive(count) && is_positive(time)){
                rimuovi_spazi(rescuer_name);
                //cerca il tipo 
                rescuer_type_t *rtype = find_rescuer(rescuer_name, available_types, type_count);
                //se lo trova lo aggiunge all'array
                if(rtype){ 
                    rescuer_request_t *req = &etype->rescuers[etype->rescuers_req_number++];
                    req->type = rtype;
                    req->required_count = count;
                    req->time_to_manage = time;
                }else {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Tipo soccorritore non riconosciuto: '%s'", rescuer_name);
                    log_parsing_event(filename, "ERRORE_FORMATO", msg);
                }


            }else if(!error_log){
                log_parsing_event(filename, "ERRORE_FORMATO", line);
                error_log = 1;
            }
            
            token = strtok(NULL, ";");
        }

        if (etype->rescuers_req_number == 0) {
            log_parsing_event(filename, "ERRORE_FORMATO", "Nessun soccorritore valido trovato");
            data.count--; // annulla aggiunta di quuesto tipo
            free(etype->rescuers);
            free(etype->emergency_desc);
            continue;
        }

        char msg[MSG_LEN];
        snprintf(msg, sizeof(msg), "Tipo emergenza '%s', priorità %d", name, priority);
        log_parsing_event(filename, "RIGA_VALIDA", msg);

        if (data.count >= capacity) {
            capacity *= 2;
            SAFE_REALLOC(data.types, capacity * sizeof(emergency_type_t));
        }

    }
    char msg[MSG_LEN];
    snprintf(msg, sizeof(msg), "Parsing completato con %d tipi", data.count);
    log_parsing_event(filename, "FINE", msg);

    fclose(fp);
    return data;

}