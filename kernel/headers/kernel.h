#ifndef KERNEL_H
#define KERNEL_H

/////////////////////////////// Includes ///////////////////////////////
#include "../../utils/headers/sockets.h"
#include "procesos.h"
#include "syscalls.h"
#include "planificadores.h"

/////////////////////////////// Declaracion de variables globales ///////////////////////////////
// Logger
extern t_log* kernel_log;

// Cronometro para MT en PCB
extern t_temporal* tiempo_estado_actual;
extern t_dictionary* tiempos_por_pid;

// Cronometro para MT en PCB
extern t_temporal* tiempo_estado_actual;
extern t_dictionary* tiempos_por_pid;
extern t_dictionary* archivo_por_pcb;

// Sockets
extern int fd_memoria;
extern int fd_dispatch;
extern int fd_interrupt;

// Config
extern t_config* kernel_config;
extern char* IP_MEMORIA;
extern char* PUERTO_MEMORIA;
extern char* PUERTO_ESCUCHA_DISPATCH;
extern char* PUERTO_ESCUCHA_INTERRUPT;
extern char* PUERTO_ESCUCHA_IO;
extern char* ALGORITMO_CORTO_PLAZO;
extern char* ALGORITMO_INGRESO_A_READY;
extern char* ALFA;
extern char* TIEMPO_SUSPENSION;
extern char* ESTIMACION_INICIAL;
extern char* LOG_LEVEL;

// Colas de Estados
extern t_list* cola_new;
extern t_list* cola_ready;
extern t_list* cola_running;
extern t_list* cola_blocked;
extern t_list* cola_susp_ready;
extern t_list* cola_susp_blocked;
extern t_list* cola_exit;
extern t_list* cola_procesos;
extern t_list* pcbs_bloqueados_por_io;

// Listas y semaforos de CPUs y IOs conectadas
extern t_list* lista_cpus;
extern pthread_mutex_t mutex_lista_cpus;
extern t_list* lista_ios;
extern pthread_mutex_t mutex_ios;

// Conexiones minimas
extern bool conectado_cpu;
extern bool conectado_io;
extern pthread_mutex_t mutex_conexiones;

/////////////////////////////// Prototipos ///////////////////////////////
void iniciar_config_kernel(void);
void iniciar_logger_kernel(void);
void iniciar_logger_kernel_debug(void);
void iniciar_diccionario_tiempos(void);
void* hilo_cliente_memoria(void* _);
void* hilo_servidor_dispatch(void* _);
void* hilo_servidor_interrupt(void* _);
void* hilo_servidor_io(void* _);
void iniciar_estados_kernel(void);
void iniciar_sincronizacion_kernel(void);
void terminar_kernel(void);
bool cpu_por_fd_simple(void* ptr, int fd);
uint16_t get_pid_from_cpu(int fd, op_code instruccion);
void* atender_cpu_dispatch(void* arg);
void* atender_cpu_interrupt(void* arg);
void* atender_io(void* arg);

#endif /* KERNEL_H */
