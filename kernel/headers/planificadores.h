#ifndef PLANIFICADORES_H
#define PLANIFICADORES_H

#define _GNU_SOURCE  // Para usleep() y otras funciones POSIX

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
extern pthread_mutex_t mutex_cola_procesos;
extern pthread_mutex_t mutex_cola_rechazados;
extern pthread_mutex_t mutex_pcbs_esperando_io;
extern pthread_mutex_t mutex_cola_interrupciones;
extern sem_t sem_proceso_a_new;
extern sem_t sem_proceso_a_susp_ready;
extern sem_t sem_proceso_a_susp_blocked;
extern sem_t sem_proceso_a_ready;
extern sem_t sem_proceso_a_running;
extern sem_t sem_proceso_a_blocked;
extern sem_t sem_proceso_a_exit;
extern sem_t sem_proceso_a_rechazados;
extern sem_t sem_susp_ready_vacia;
extern sem_t sem_finalizacion_de_proceso;
extern sem_t sem_cpu_disponible;
extern sem_t sem_replanificar_srt;
extern sem_t sem_interrupciones;

// Prototipos para comunicación efímera con Memoria

t_respuesta suspender_proceso_en_memoria(int pid);
t_respuesta desuspender_proceso_en_memoria(int pid);

// Funciones para manejo de procesos rechazados
void encolar_proceso_rechazado(t_pcb* pcb);

/////////////////////////// Planificacion de Largo Plazo ////////////////////////////

// Add enum for planificador states
typedef enum {
    STOP,
    RUNNING
} estado_planificador;

//////////////////////////////////////////////////////////////
t_pcb* elegir_por_fifo();

void* menor_rafaga(void* a, void* b);
t_pcb* elegir_por_sjf();

t_pcb* elegir_por_srt();

void* menor_rafaga_restante(void* a, void* b);

void dispatch(t_pcb* proceso_a_ejecutar);

bool interrupt(cpu *cpu_a_desalojar, t_pcb *proceso_a_ejecutar);

double get_time();

void* planificador_largo_plazo(void* arg);

void activar_planificador_largo_plazo(void);

void iniciar_planificadores(void);

void iniciar_interrupt_handler(void);

void* interrupt_handler(void* arg);

void solicitar_replanificacion_srt(void);

void* planificador_largo_plazo(void* arg);

void* menor_tamanio(void* a, void* b);

t_pcb* elegir_por_pmcp(); // Esta solo elige PMCP en la cola de NEW
t_pcb* elegir_por_pmcp_en_cola(t_list* cola); // Es para elegir procesos por PMCP en la cola de SUSP_READY

void* gestionar_exit(void* arg);
void* planificador_corto_plazo(void* arg);

bool hay_espacio_suficiente_memoria(int tamanio);

int obtener_fd_interrupt(int id_cpu);

void* planificador_mediano_plazo(t_pcb* pcb);
void* timer_suspension_blocked(void* arg);

#endif /* PLANIFICADORES_H */