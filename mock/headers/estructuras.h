#ifndef ESTRUCTURAS_H
#define ESTRUCTURAS_H

#include <commons/collections/list.h>
#include <pthread.h>
#include <semaphore.h>

// Estados de proceso
typedef enum {
    NEW,
    READY,
    EXEC,
    BLOCKED,
    SUSP_READY,
    SUSP_BLOCKED,
    EXIT_ESTADO
} t_estado;

// Estructura PCB
typedef struct {
    int PID;
    t_estado Estado;
    int tamanio_memoria;
    char* path;
} t_pcb;

#endif // ESTRUCTURAS_H 