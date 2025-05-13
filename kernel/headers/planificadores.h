#ifndef PLANIFICADORES_H
#define PLANIFICADORES_H

/////////////////////////////// Includes ///////////////////////////////
#include "kernel.h"
#include "types.h"
#include <semaphore.h>

/////////////////////////////// Prototipos ///////////////////////////////

// Semaforos de planificacion
extern pthread_mutex_t mutex_cola_new;
extern pthread_mutex_t mutex_cola_susp_ready;
extern pthread_mutex_t mutex_cola_susp_blocked;
extern pthread_mutex_t mutex_cola_ready;
extern pthread_mutex_t mutex_cola_running;
extern pthread_mutex_t mutex_cola_blocked;
extern pthread_mutex_t mutex_cola_exit;
extern sem_t sem_proceso_a_new;
extern sem_t sem_proceso_a_susp_ready;
extern sem_t sem_proceso_a_susp_blocked;
extern sem_t sem_proceso_a_ready;
extern sem_t sem_proceso_a_running;
extern sem_t sem_proceso_a_blocked;
extern sem_t sem_proceso_a_exit;
extern sem_t sem_susp_ready_vacia;
extern sem_t sem_finalizacion_de_proceso;

void iniciar_planificador_corto_plazo(char* algoritmo);
void dispatch(t_pcb* proceso_a_ejecutar);
t_pcb* elegir_por_fifo(void);
void* menor_rafaga(void* a, void* b);
t_pcb* elegir_por_sjf(void);
t_pcb* elegir_por_srt(void);
double get_time(void);
void iniciar_planificador_largo_plazo(void);
void* planificar_FIFO_lp(void* arg);
void* planificar_PMCP_lp(void* arg);
void* menor_tamanio(void* a, void* b);
t_pcb* elegir_por_pmcp(void);
void* gestionar_exit(void* arg);

#endif /* PLANIFICADORES_H */