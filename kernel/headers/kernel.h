#ifndef KERNEL_H
#define KERNEL_H

/////////////////////////////// Includes ///////////////////////////////
#include "../../utils/headers/sockets.h"
#include "procesos.h"
#include "syscalls.h"
#include "planificadores.h"
#include "IOKernel.h"
#include "CPUKernel.h"
#include <signal.h>
#include <unistd.h>
#include <commons/collections/queue.h>
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
extern double ALFA;
extern double TIEMPO_SUSPENSION;
extern double ESTIMACION_INICIAL;
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
extern t_list* cola_rechazados;
extern t_list* pcbs_bloqueados_por_dump_memory;
extern t_list* pcbs_esperando_io;
extern t_queue* cola_interrupciones;

// Listas y semaforos de CPUs y IOs conectadas
extern t_list* lista_cpus;
extern t_list* lista_ios;

// Conexiones
extern pthread_mutex_t mutex_lista_cpus;
extern pthread_mutex_t mutex_ios;

// Conexiones minimas
extern bool conectado_cpu;
extern bool conectado_io;
extern bool conectado_memoria;
extern pthread_mutex_t mutex_conexiones;

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                       INICIALIZACIONES                                       //
//////////////////////////////////////////////////////////////////////////////////////////////////

void iniciar_config_kernel();

void iniciar_logger_kernel();

void iniciar_logger_kernel_debug();

void iniciar_estados_kernel();

void iniciar_sincronizacion_kernel();

void iniciar_diccionario_tiempos();

void iniciar_diccionario_archivos_por_pcb();

void terminar_kernel();

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                            MEMORIA                                           //
//////////////////////////////////////////////////////////////////////////////////////////////////

void* hilo_cliente_memoria(void* _);

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                         CPU DISPATCH                                         //
//////////////////////////////////////////////////////////////////////////////////////////////////

void* hilo_servidor_dispatch(void* _);
void* atender_cpu_dispatch(void* arg);

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                         CPU INTERRUPT                                        //
//////////////////////////////////////////////////////////////////////////////////////////////////

void* hilo_servidor_interrupt(void* _);

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                              IO                                              //
//////////////////////////////////////////////////////////////////////////////////////////////////

void* hilo_servidor_io(void* _);

void* atender_io(void* arg);

void verificar_procesos_bloqueados(io* io);

t_pcb_io* obtener_pcb_esperando_io(char* nombre_io);

void asignar_proceso(io* dispositivo, t_pcb_io* proceso);

void exit_procesos_relacionados(io* io);

#endif /* KERNEL_H */
