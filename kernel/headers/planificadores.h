#ifndef PLANIFICADORES_H
#define PLANIFICADORES_H

/////////////////////////////// Includes ///////////////////////////////
#include "kernel.h"
#include "types.h"

/////////////////////////////// Prototipos ///////////////////////////////

// Semaforos de planificacion
extern pthread_mutex_t mutex_cola_new;
extern pthread_mutex_t mutex_cola_susp_ready;
extern pthread_mutex_t mutex_cola_susp_blocked;
extern pthread_mutex_t mutex_cola_ready;
extern pthread_mutex_t mutex_cola_running;
extern pthread_mutex_t mutex_cola_blocked;
extern pthread_mutex_t mutex_cola_exit;


void iniciar_planificador_corto_plazo(char* algoritmo);
void dispatch(t_pcb* proceso_a_ejecutar);
t_pcb* elegir_por_fifo(void);
void* menor_rafaga(void* a, void* b);
t_pcb* elegir_por_sjf(void);
t_pcb* elegir_por_srt(void);
double get_time(void);
void fin_io(t_pcb* pcb);
void iniciar_planificador_largo_plazo();
void* planificar_FIFO_lp();
void* planificar_PMCP_lp();
void* menor_tamanio(void* a, void* b);
t_pcb* elegir_por_pmcp(void);

#endif /* PLANIFICADORES_H */