#ifndef MAIN_H_
#define MAIN_H_
#include "sockets.h"

t_log* log_cpu;

//void atender_cliente(void*);
void iterator(char* value);
int recibir_kernel_dispatch(/*int fd_kernel_dispatch*/);    //  comento el parametro porque en la definicion no lo tiene, no compila
int recibir_kernel_interrupt(/*int fd_kernel_interrupt*/);  //  comento el parametro porque en la definicion no lo tiene, no compila
void terminar_programa(void);

#endif