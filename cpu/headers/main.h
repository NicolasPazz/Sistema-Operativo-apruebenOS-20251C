#ifndef MAIN_H_
#define MAIN_H_
#include "sockets.h"




t_log* log_cpu;

void atender_cliente(void*);
void iterator(char* value);
//void establecer_conexion_cpu_memoria();
void* recibir_kernel_dispatch(void* arg);
void* recibir_kernel_interrupt(void* arg);
















#endif