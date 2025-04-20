#include "../headers/monitor_memoria.h"

extern t_log* logger; // por funciones de debug
extern t_config_memoria* cfg;
extern t_list* segmentos_libres;
extern t_list* segmentos_usados;

extern t_list* tp_patotas;
//extern frame_t* tabla_frames;

extern void* memoria_principal;

/// mutex y semaforos

pthread_mutex_t MUTEX_SEGMENTOS_LIBRES;
pthread_mutex_t MUTEX_SEGMENTOS_USADOS;
pthread_mutex_t MUTEX_FRAMO;

pthread_mutex_t MUTEX_MP;
pthread_mutex_t MUTEX_TS_PATOTAS;
pthread_mutex_t MUTEX_TS_TRIPULANTES;

pthread_mutex_t MUTEX_TP_PATOTAS;
pthread_mutex_t MUTEX_TID_PID_LOOKUP;

pthread_mutex_t MUTEX_MP_BUSY;

sem_t SEM_INICIAR_SELF_EN_PATOTA;
sem_t SEM_COMPACTACION_START;
sem_t SEM_COMPACTACION_DONE;


void iniciar_mutex() {
    pthread_mutex_init(&MUTEX_SEGMENTOS_LIBRES, NULL);
    pthread_mutex_init(&MUTEX_SEGMENTOS_USADOS, NULL);
    pthread_mutex_init(&MUTEX_FRAMO, NULL);
    pthread_mutex_init(&MUTEX_MP, NULL);
    pthread_mutex_init(&MUTEX_TS_PATOTAS, NULL);
    pthread_mutex_init(&MUTEX_TS_TRIPULANTES, NULL);
    pthread_mutex_init(&MUTEX_TP_PATOTAS, NULL);
    pthread_mutex_init(&MUTEX_TID_PID_LOOKUP, NULL);
    pthread_mutex_init(&MUTEX_MP_BUSY, NULL);

    sem_init(&SEM_INICIAR_SELF_EN_PATOTA, 0, 0);
    sem_init(&SEM_COMPACTACION_START, 0, 0);
    sem_init(&SEM_COMPACTACION_DONE, 0, 0);
}