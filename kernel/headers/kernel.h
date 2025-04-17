#ifndef KERNEL_H
#define KERNEL_H

/////////////////////////////// Includes ///////////////////////////////
#include "../../utils/headers/sockets.h"
#include "procesos.h"
#include "syscalls.h"
#include "planificadores.h"

/////////////////////////////// Declaración de variables globales ///////////////////////////////
// Logger
extern t_log* kernel_log;

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

/////////////////////////////// Prototipos ///////////////////////////////
void iniciar_config_kernel(void);
void iniciar_logger_kernel(void);
void iniciar_logger_kernel_debug(void);
void* hilo_cliente_memoria(void* _);
void* hilo_servidor_dispatch(void* _);
void* hilo_servidor_interrupt(void* _);
void* hilo_servidor_io(void* _);
void iniciar_estados_kernel(void);

#endif /* KERNEL_H */
