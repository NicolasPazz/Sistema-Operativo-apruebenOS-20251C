#include "../headers/syscalls.h"

/////////////////////////////// Funciones ///////////////////////////////
void INIT_PROC(char* nombre_archivo, uint16_t tam_memoria) {
    t_pcb* nuevo_pcb = malloc(sizeof(t_pcb));

    // Inicializar el todo el PCB en 0 (por ahora)
    nuevo_pcb->PID = list_size(cola_procesos); // El PID es númericamente igual a la posición del PCB dentro de la cola_procesos
    nuevo_pcb->PC = 0;
    for (int i = 0; i < 7; i++) {
        nuevo_pcb->ME[i] = 0;
        nuevo_pcb->MT[i] = 0;
    }
    nuevo_pcb->Estado = 0;

    list_add(cola_new, nuevo_pcb);
    list_add(cola_procesos, nuevo_pcb);
}

void DUMP_MEMORY();

void IO(char* nombre_io, uint16_t tiempo_a_usar);
