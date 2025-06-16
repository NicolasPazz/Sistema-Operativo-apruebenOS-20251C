#ifndef INITS_H
#define INITS_H

#include <pthread.h>
#include <semaphore.h>
#include <commons/collections/list.h>

// Mutexes para las colas
extern pthread_mutex_t mutex_cola_new;
extern pthread_mutex_t mutex_cola_susp_ready;
extern pthread_mutex_t mutex_cola_susp_blocked;
extern pthread_mutex_t mutex_cola_ready;
extern pthread_mutex_t mutex_cola_running;
extern pthread_mutex_t mutex_cola_blocked;
extern pthread_mutex_t mutex_cola_exit;

// Semáforos para sincronización
extern sem_t sem_proceso_a_new;
extern sem_t sem_proceso_a_susp_ready;
extern sem_t sem_proceso_a_susp_blocked;
extern sem_t sem_proceso_a_ready;
extern sem_t sem_proceso_a_running;
extern sem_t sem_proceso_a_blocked;
extern sem_t sem_proceso_a_exit;
extern sem_t sem_susp_ready_vacia;
extern sem_t sem_finalizacion_de_proceso;

// Colas de procesos
extern t_list* cola_new;
extern t_list* cola_ready;
extern t_list* cola_running;
extern t_list* cola_blocked;
extern t_list* cola_susp_ready;
extern t_list* cola_susp_blocked;
extern t_list* cola_exit;
extern t_list* cola_procesos;

// Funciones
void iniciar_sincronizacion_mock();
void iniciar_estados_mock();
void terminar_mock();

#endif // INITS_H 