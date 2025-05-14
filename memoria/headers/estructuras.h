#ifndef ESTRUCTURAS_H
#define ESTRUCTURAS_H

#include <stdint.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include <stdbool.h>
#include "../../utils/headers/sockets.h"

// Estructura para almacenar el conjunto de instrucciones de un proceso
typedef struct {
    uint32_t pid;
    t_list* instructions;  // Lista de t_extended_instruccion*
} t_process_instructions;

// Estructura para representar la información de un proceso en memoria
typedef struct {
    uint32_t pid;
    uint32_t size;
    bool is_active;
    // Métricas por proceso
    uint32_t page_table_accesses;
    uint32_t instructions_requested;
    uint32_t swap_writes;
    uint32_t memory_loads;
    uint32_t memory_reads;
    uint32_t memory_writes;
} t_process_info;

#endif
