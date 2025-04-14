#ifndef PROCESOS_H
#define PROCESOS_H

/////////////////////////////// Includes ///////////////////////////////
#include "kernel.h"
#include <stdint.h>
#include <stdio.h>

/////////////////////////////// Estructuras ///////////////////////////////
typedef struct PCB {
    uint16_t PID;
    uint16_t PC;
    uint16_t ME[7];
    uint16_t MT[7];
    uint16_t Estado;
} t_pcb;

typedef enum Estados {
    NEW,
    READY,
    EXEC,
    BLOCKED,
    SUSP_READY,
    SUSP_BLOCKED,
    EXIT_ESTADO
} Estados;


/////////////////////////////// Prototipos ///////////////////////////////
void mostrar_pcb(t_pcb PCB); // Muestra por pantalla los valores de un PCB
const char* estado_to_string(Estados estado); // Asocia el valor del enum con el nombre del estado (Para imprimir el nombre en vez de un número)
void cambiar_estado_pcb(t_pcb* PCB, t_list* estado_actual, t_list* nuevo_estado);

#endif /* PROCESOS_H */