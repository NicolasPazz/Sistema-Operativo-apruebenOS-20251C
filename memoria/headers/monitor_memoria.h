#ifndef MONITOR_MEMORIA_H_
#define MONITOR_MEMORIA_H_

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include <commons/temporal.h>

#include "init_memoria.h"
#include "estructuras.h"

// init
void iniciar_mutex();
void finalizar_mutex();


#endif