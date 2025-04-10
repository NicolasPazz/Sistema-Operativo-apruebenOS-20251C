#ifndef KERNEL_H
#define KERNEL_H

/////////////////////////////// Includes ///////////////////////////////
#include "../../utils/headers/sockets.h"

/////////////////////////////// Declaración de variables globales ///////////////////////////////
extern t_log* kernel_log;
extern t_config* kernel_config;

extern int fd_memoria;
extern int fd_dispatch;
extern int fd_interrupt;

extern char* IP_MEMORIA;
extern char* PUERTO_MEMORIA;
extern char* PUERTO_ESCUCHA_DISPATCH;
extern char* PUERTO_ESCUCHA_INTERRUPT;
extern char* PUERTO_ESCUCHA_IO;
extern char* ALGORITMO_PLANIFICACION;
extern char* ALGORITMO_COLA_NEW;
extern char* ALFA;
extern char* TIEMPO_SUSPENSION;
extern char* LOG_LEVEL;

/////////////////////////////// Prototipos ///////////////////////////////
void iniciar_config_kernel(void);
void iniciar_logger_kernel(void);
void iniciar_conexiones_kernel(void);

#endif /* KERNEL_H */
