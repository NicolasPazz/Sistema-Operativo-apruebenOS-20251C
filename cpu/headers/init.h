#ifndef CPU_H
#define CPU_H

/////////////////////////////// Includes ///////////////////////////////
#include "sockets.h"

/////////////////////////////// Declaración de variables globales ///////////////////////////////
extern t_log* cpu_log;
extern t_config* cpu_config;

extern int fd_memoria;
extern int fd_kernel_dispatch;
extern int fd_kernel_interrupt;

extern char* IP_MEMORIA;
extern char* PUERTO_MEMORIA;
extern char* IP_KERNEL;
extern char* PUERTO_KERNEL_DISPATCH;
extern char* PUERTO_KERNEL_INTERRUPT;
extern char* ENTRADAS_TLB;
extern char* REEMPLAZO_TLB;
extern char* ENTRADAS_CACHE;
extern char* REEMPLAZO_CACHE;
extern char* RETARDO_CACHE;
extern char* LOG_LEVEL;

/////////////////////////////// Prototipos ///////////////////////////////
void leer_config_cpu(void);
void iniciar_logger_cpu(void);
//void iniciar_conexiones_cpu(void);
void conectar_kernel_dispatch();
void conectar_kernel_interrupt();
int realizar_handshake(char* ip, char* puerto, char* quien_realiza_solicitud, char* con_quien_se_conecta);

#endif /* CPU_H */
