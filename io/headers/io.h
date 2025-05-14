#ifndef IO_H
#define IO_H

/////////////////////////////// Includes ///////////////////////////////
#include "../../utils/headers/sockets.h"

/////////////////////////////// Declaracion de variables globales ///////////////////////////////
extern t_log* io_log;
extern t_config* io_config;

extern int fd_kernel_io;

extern char* IP_KERNEL;
extern char* PUERTO_KERNEL;
extern char* LOG_LEVEL;

/////////////////////////////// Prototipos ///////////////////////////////
void iniciar_config_io(void);
void iniciar_logger_io(void);
void iniciar_conexiones_io(char* nombre_io);

#endif /* IO_H */
