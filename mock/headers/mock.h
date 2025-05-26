#ifndef MOCK_H
#define MOCK_H

/////////////////////////////// Includes ///////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>
#include <signal.h>
#include "../../utils/headers/sockets.h"
#include "../../utils/headers/utils.h"

/////////////////////////////// Declaracion de variables globales ///////////////////////////////

// Logger
extern t_log* mock_log;

// Conexiones a módulos
extern int fd_memoria;
extern int fd_kernel_dispatch;
extern int fd_kernel_interrupt;
extern int fd_kernel_io;

// (Opcional) Conexiones hacia CPU para simular Kernel
extern int fd_cpu_dispatch;
extern int fd_cpu_interrupt;

/////////////////////////////// Prototipos ///////////////////////////////

#endif /* MOCK_H */
