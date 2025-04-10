#ifndef IO_H
#define IO_H

/////////////////////////////// Includes ///////////////////////////////
#include "../../utils/headers/sockets.h"

/////////////////////////////// Declaración de variables globales ///////////////////////////////
extern t_log* io_log;
extern t_config* io_config;

extern int fd_kernel;

extern char* IP_KERNEL;
extern char* PUERTO_KERNEL;
extern char* LOG_LEVEL;

/////////////////////////////// Prototipos ///////////////////////////////
void iniciar_config_io(void);
void iniciar_logger_io(void);
void iniciar_conexiones_io(void);

#endif /* IO_H */
