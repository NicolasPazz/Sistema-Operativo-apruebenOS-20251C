#ifndef MONITOR_MEMORIA_H_
#define MONITOR_MEMORIA_H_

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <commons/temporal.h>

#include "init_memoria.h"
#include "estructuras.h"

// Funciones de inicialización/finalización del monitor
void iniciar_mutex(void);
void finalizar_mutex(void);

#endif