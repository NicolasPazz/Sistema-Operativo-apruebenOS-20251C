#ifndef MAIN_H_
#define MAIN_H_
#include "sockets.h"

extern t_log* log_cpu;
extern pthread_mutex_t mutex_estado_proceso;
extern pthread_mutex_t mutex_tlb;
extern pthread_mutex_t mutex_cache;
void iterator(char* value);
void* recibir_kernel_dispatch(void* arg);    
void* recibir_kernel_interrupt(void* arg);
void terminar_programa(void);

#endif