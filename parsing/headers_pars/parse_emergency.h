#ifndef PARSE_EMERGENCY_TYPES_H
#define PARSE_EMERGENCY_TYPES_H

#include "struct.h"

typedef struct {
    emergency_type_t *types;
    int count;
} emergency_data_t;

emergency_data_t parse_emergency_types_config(const char *filename, rescuer_type_t *available_types, int type_count);


#endif
