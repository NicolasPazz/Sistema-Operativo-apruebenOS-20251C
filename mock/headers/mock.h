#ifndef MOCK_H
#define MOCK_H

/////////////////////////////// Includes ///////////////////////////////
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../../utils/headers/sockets.h"
#include <commons/log.h>

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
bool init_proc_mock(int cliente_socket, int pid, int tamanio, char* instrucciones_path);

#endif /* MOCK_H */
