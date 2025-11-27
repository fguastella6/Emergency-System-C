#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers_pars/parse_rescuers.h"
#include "macro.h"
#include "utils.h"

// Funzione che legge il file rescuer.conf e restituisce un array di tipi e array di mezzi
rescuers_data_t parse_rescuers_config(const char *filename){
    FILE *fp;
    SAFE_FOPEN(fp,filename ,"r", filename);

   
    rescuers_data_t data = {0};
    char line[MAX_LINE];
    int type_capacity = 10, twin_capacity = 50;

    SAFE_MALLOC(data.types, type_capacity * sizeof(rescuer_type_t));
    SAFE_MALLOC(data.twins, twin_capacity * sizeof(rescuer_digital_twin_t));
    
    while (fgets(line, sizeof(line), fp)){
        char name[64];
        int num, speed, x, y;

        if(sscanf(line, "[%[^]]][%d][%d][%d;%d]", name, &num, &speed, &x, &y) != 5) {
            log_parsing_event(filename, "ERRORE_FORMATO", line);
            //fprintf(stderr, "Formato non valido: %s\n", line);
            continue;
        }
        
        if (!is_positive(num) || !is_positive(speed) || !is_valid_coordinate(x, y)) {
            log_parsing_event(filename, "ERRORE_FORMATO", "Valori non validi");
            continue;
        }           
        log_parsing_event(filename, "RIGA_VALIDA", line);

        if(data.type_count >= type_capacity){ //se abbiamo ancora spezio nell'array types
            type_capacity *=2;
            SAFE_REALLOC(data.types, type_capacity * sizeof(rescuer_type_t));
        }

        //creazione di un nuovo tipo di soccorritore nell'array types
        rescuer_type_t *r = &data.types[data.type_count++];
        r->rescuer_type_name = my_strdup(name);
        r->speed = speed;
        r->x = x;
        r->y = y;

        //creazione dei gemelli digitalil
        for (int i = 0; i < num; ++i) {
            if (data.twin_count >= twin_capacity) {
                twin_capacity *= 2;
                SAFE_REALLOC(data.twins, twin_capacity * sizeof(rescuer_digital_twin_t));
            }
            rescuer_digital_twin_t *twin = &data.twins[data.twin_count];
            twin->id = data.twin_count;
            twin->x = x;
            twin->y = y;
            twin->rescuer = r;
            twin->status = IDLE;
            data.twin_count++;

        }
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "Parsing completato con %d tipi", data.type_count);
    log_parsing_event(filename, "FINE", msg);

    fclose(fp);
    return data;
}