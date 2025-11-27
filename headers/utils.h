#ifndef UTILS_H
#define UTILS_H
#include "struct.h"
#include <signal.h>
extern volatile sig_atomic_t g_shutdown;

int distanza_manhattan(int x1, int y1, int x2, int y2);
char *my_strdup(const char *s);
int is_positive(int value);
int is_valid_coordinate(int x, int y);
int is_valid_delay(int delay);
int is_nonempty_string(const char *s);
void rimuovi_spazi(char *str);
int emergenza_terminata(const emergency_t *em);
int ceil_div(int a, int b);
int eta_secs(rescuer_digital_twin_t *dt, int x, int y);
int deadline_secs(short priority);
int count_idle(rescuer_digital_twin_t *twins, int n, const char *type_name, emergency_t *em);
int sleep_2(emergency_t *em, int seconds);

#endif