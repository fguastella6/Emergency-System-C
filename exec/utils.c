#include <stdlib.h>
#include <string.h>
#include "struct.h"
#include "utils.h"
#include "macro.h"
#include <threads.h>
#include <signal.h>
volatile sig_atomic_t g_shutdown;


int distanza_manhattan(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}


//conta i soccorritori di un tipo che sono IDLE
int count_idle(rescuer_digital_twin_t *twins, int n, const char *type_name, emergency_t *em){
    int c = 0;
    for(int i = 0; i < n; i++){
        rescuer_digital_twin_t *dt = &twins[i];
        if (!dt->rescuer) continue;
        if (strcmp(dt->rescuer->rescuer_type_name, type_name) != 0) continue;

        if (dt->status == IDLE) { c++; continue; }
        if (dt->owner == em && dt->status == EN_ROUTE_TO_SCENE) { c++; continue; }
    }
    return c;
}
char *my_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy;
    SAFE_MALLOC(copy, len);
    memcpy(copy, s, len);
    return copy;

}

int is_positive(int value) {
    return value >= 0;
}

int is_valid_coordinate(int x, int y) {
    return x >= 0 && y >= 0;
}

int is_valid_delay(int delay) {
    return delay >= 0;
}

int is_nonempty_string(const char *s) {
    return s != NULL && s[0] != '\0';
}

void rimuovi_spazi(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src != ' ' && *src != '\t' && *src != '\n' && *src != '\r')
            *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

// Interrompe il thread se l’emergenza è stata preemptata, annullata o ha raggiunto timeout
int emergenza_terminata(const emergency_t *em) {
    return (em->status == PAUSED || em->status == TIMEOUT || em->status == CANCELED);
}

int ceil_div(int a, int b){
    if(b <= 0) return 0; //difesa
    return (a+b -1) /b;
}

int eta_secs(rescuer_digital_twin_t *dt, int x, int y) {
    if (!dt || !dt->rescuer || dt->rescuer->speed <= 0)
        return -1;  // ETA non calcolabile

    int dist = distanza_manhattan(dt->x, dt->y, x, y);
    return ceil_div(dist, dt->rescuer->speed);
}

int deadline_secs(short priority){
    switch(priority){
        case 2: return 10; //alta priorita 
        case 1: return 30; //media priorita
        default: return -1; //nessun limite
    }
}

int sleep_2(emergency_t *em, int seconds){
    const int step_ms = 100; // 0.1s
    int elapsed_ms = 0;
    while (elapsed_ms < seconds * 1000) {
        if (g_shutdown) return -1;
        if (em && emergenza_terminata(em)) return -2;

        struct timespec ts = { .tv_sec = 0, .tv_nsec = step_ms * 1000000L };
        thrd_sleep(&ts, NULL);
        elapsed_ms += step_ms;
    }
    return 0;
}