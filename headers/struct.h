#ifndef STRUCT_H
#define STRUCT_H

#include <time.h>
#define EMERGENCY_NAME_LENGTH 64
struct emergency_t;

//rescuers
typedef enum {
    IDLE, EN_ROUTE_TO_SCENE, ON_SCENE, RETURNING_TO_BASE
}rescuer_status_t;

typedef struct{
    char * rescuer_type_name;
    int speed;
    int x;
    int y;
}rescuer_type_t;

typedef struct 
{
    int id;
    int x;
    int y;
    rescuer_status_t status;
    rescuer_type_t * rescuer;
    struct emergency_t  *owner; // proprietario corrente (se aseegnato)
}rescuer_digital_twin_t;


//emergency
typedef enum{
    WAITING, ASSIGNED, IN_PROGRESS, PAUSED, COMPLETED, CANCELED, TIMEOUT
}emergency_status_t;

//indicare quante unità di soccorso servono, e per quanto tempo.
typedef struct {
    rescuer_type_t * type;  //puntatore al tipo di soccorritore
    int required_count;     //# istanze
    int time_to_manage;     //Tempo di gestione (sec)
}rescuer_request_t;

typedef struct {
    short priority;
    char * emergency_desc;
    rescuer_request_t * rescuers;
    int rescuers_req_number;
}emergency_type_t;

//messaggio ricevuto dalla coda
typedef struct {
    char emergency_name[EMERGENCY_NAME_LENGTH];
    int x;
    int y;
    time_t timestamp;
}emergency_request_t;

//rappresentare un intervento in corso durante l'exec
typedef struct emergency_t{
    char id[128];
    emergency_type_t type;  //tipo definito nel parese
    short current_priority;
    emergency_status_t status;
    int x;
    int y;
    time_t time;
    time_t request_timestamp;
    int rescuer_count;
    rescuer_digital_twin_t* rescuers_dt;
    time_t waiting_start_time; // inizio attesa in stato WAITING

}emergency_t;

#endif
//en

#ifndef TYPES_H
#define TYPES_H

typedef struct {
    char *queue_name;
    int height;
    int width;
}env_config_t;

#endif