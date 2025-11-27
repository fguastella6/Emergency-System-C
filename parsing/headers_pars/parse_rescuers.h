#ifndef PARSE_RESCUERS_H
#define PARSE_RESCUERS_H

#include "struct.h"

typedef struct {
    rescuer_type_t *types;
    rescuer_digital_twin_t *twins;
    int type_count;
    int twin_count;
} rescuers_data_t;

rescuers_data_t parse_rescuers_config(const char *filename);
#endif