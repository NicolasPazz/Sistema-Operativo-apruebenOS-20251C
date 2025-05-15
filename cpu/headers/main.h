#ifndef MAIN_H_
#define MAIN_H_
#include "sockets.h"

t_log* log_cpu;

//void atender_cliente(void*);
void iterator(char* value);
int recibir_kernel_dispatch();    
int recibir_kernel_interrupt();
void terminar_programa(void);

#endif