/* exec/logger.c - LOGGER CONDIVISO */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <threads.h>
#include <string.h>
#include "macro.h" 

// Variabili statiche (nascoste agli altri file)
static FILE *g_log_file = NULL;
static mtx_t g_log_mtx;
static int g_is_initialized = 0;

const char *logLevelStr(int level) {
    switch(level) {
        case LL_DEBUG: return ".";
        case LL_INFO:  return "*";
        case LL_WARN:  return "#";
        case LL_ERR:   return "!";
        default:       return "?";
    }
}

// Inizializzazione (da chiamare sia nel main del server che del client)
void init_logger(const char *filename) {
    if (g_is_initialized) return;

    // "a" = append mode. Scritture piccole sono atomiche su POSIX.
    g_log_file = fopen(filename, "a");
    if (!g_log_file) {
        perror("Impossibile aprire file di log");
        // Non usciamo, stampiamo su stderr e basta
    }
    mtx_init(&g_log_mtx, mtx_plain);
    g_is_initialized = 1;
}

void close_logger(void) {
    if (g_log_file) fclose(g_log_file);
    if (g_is_initialized) mtx_destroy(&g_log_mtx);
    g_is_initialized = 0;
}

// Funzione generica di log
void serverLog(int level, const char *fmt, ...) {
    if (!g_log_file) return;

    va_list ap;
    
    // Lock per thread-safety (nel caso il client o server siano multithread)
    mtx_lock(&g_log_mtx);

    // 1. Timestamp
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%d %b %H:%M:%S", tm);

    // 2. Header: [PID] Data Livello
    fprintf(g_log_file, "[%d] %s %s ", (int)getpid(), buf, logLevelStr(level));

    // 3. Messaggio
    va_start(ap, fmt);
    vfprintf(g_log_file, fmt, ap);
    va_end(ap);

    fprintf(g_log_file, "\n");
    fflush(g_log_file); // Importante per vedere subito i log

    mtx_unlock(&g_log_mtx);
}

// Funzioni di compatibilità per i Parser
void log_parsing_event(const char *file_id, const char *evento, const char *contenuto) {
    serverLog(LL_INFO, "[PARSING] %s -> %s: %s", file_id, evento, contenuto);
}

void log_event(const char *id, const char *category, const char *message) {
    serverLog(LL_INFO, "[%s] [%s] %s", id, category, message);
}