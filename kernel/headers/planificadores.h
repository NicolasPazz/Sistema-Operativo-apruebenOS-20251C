#ifndef PLANIFICADORES_H
#define PLANIFICADORES_H

#define _GNU_SOURCE // Para usleep() y otras funciones POSIX

/////////////////////////////// Includes ///////////////////////////////

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
extern pthread_mutex_t mutex_cola_procesos;
extern pthread_mutex_t mutex_pcbs_esperando_io;
extern pthread_mutex_t mutex_cola_interrupciones;
extern pthread_mutex_t mutex_planificador_lp;
extern pthread_mutex_t mutex_procesos_rechazados;
extern sem_t sem_proceso_a_new;
extern sem_t sem_proceso_a_susp_ready;
extern sem_t sem_proceso_a_susp_blocked;
extern sem_t sem_proceso_a_ready;
extern sem_t sem_proceso_a_running;
extern sem_t sem_proceso_a_blocked;
extern sem_t sem_proceso_a_exit;
extern sem_t sem_susp_ready_vacia;
extern sem_t sem_finalizacion_de_proceso;
extern sem_t sem_cpu_disponible;
extern sem_t sem_replanificar_srt;
extern sem_t sem_interrupciones;

/////////////////////////// Planificacion de Largo Plazo ////////////////////////////

// Add enum for planificador states
typedef enum
{
    STOP,
    RUNNING
} estado_planificador;

extern pthread_mutex_t mutex_planificador_lp;
extern pthread_cond_t cond_planificador_lp;
extern estado_planificador estado_planificador_lp;

//////////////////////////////////////////////////////////////

t_pcb *elegir_por_fifo(t_list * cola_a_utilizar);
void *menor_rafaga(void *a, void *b);
t_pcb *elegir_por_sjf();
t_pcb *elegir_por_srt();
void *menor_rafaga_restante(void *a, void *b);
void dispatch(t_pcb *proceso_a_ejecutar);
bool interrupt(cpu *cpu_a_desalojar, t_pcb *proceso_a_ejecutar);
double get_time();
void *planificador_largo_plazo(void *arg);
void activar_planificador_largo_plazo();
void iniciar_planificadores();
void iniciar_interrupt_handler();
void *interrupt_handler(void *arg);
void solicitar_replanificacion_srt();
void *planificador_largo_plazo(void *arg);
void *menor_tamanio(void *a, void *b);
t_pcb *elegir_por_pmcp();
void *gestionar_exit(void *arg);
void *planificador_corto_plazo(void *arg);
int obtener_fd_interrupt(int id_cpu);
void iniciar_timer_suspension(t_pcb *pcb);
void aumentar_procesos_rechazados();
void disminuir_procesos_rechazados();
void verificar_procesos_rechazados();

#endif /* PLANIFICADORES_H */