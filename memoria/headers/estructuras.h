#ifndef ESTRUCTURAS_H
#define ESTRUCTURAS_H

#include <stdint.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include <stdbool.h>
#include "../../utils/headers/sockets.h"

// Estructura para almacenar el conjunto de instrucciones de un proceso
typedef struct {
    int pid;
    t_list* instructions;  // Lista de t_extended_instruccion*
} t_process_instructions;

// Estructura para representar la información de un proceso en memoria
typedef struct {
    int pid;
    int size;
    bool is_active;
    // Métricas por proceso
    int page_table_accesses;
    int instructions_requested;
    int swap_writes;
    int memory_loads;
    int memory_reads;
    int memory_writes;
} t_process_info;

#endif
