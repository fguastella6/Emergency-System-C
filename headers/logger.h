#ifndef LOGGER_H
#define LOGGER_H
#include "struct.h"
#include <time.h>

void init_logger(const char *filename);
void close_logger();
void log_event(const char *id, const char *category, const char *message);
void log_parsing_event(const char *file_id, const char *evento, const char *contenuto);
void log_queue_event(const char *id, const emergency_request_t *req, const char *azione);
void log_emergency_state(emergency_t *e, const char *from, const char *to);
void log_rescuer_state(rescuer_digital_twin_t *dt, const char *new_state, emergency_t *e);

#endif
