#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "struct.h"
#include "server.h"
#include "utils.h"

void serverCron(void);
void registerEmergency(emergency_t *em);
void unregisterEmergency(emergency_t *em);
void assignResources(void);

#endif
