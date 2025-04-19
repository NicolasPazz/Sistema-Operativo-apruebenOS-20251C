#include "../headers/syscalls.h"

t_temporal* tiempo_estado_actual;

/////////////////////////////// Funciones ///////////////////////////////
void INIT_PROC(char* nombre_archivo, uint16_t tam_memoria) {
    t_pcb* nuevo_pcb = malloc(sizeof(t_pcb));

    // Inicializar todo el PCB en 0 (por ahora)
    nuevo_pcb->PID = list_size(cola_procesos); // El PID es númericamente igual a la posición del PCB dentro de la cola_procesos
    nuevo_pcb->PC = 0;
    for (int i = 0; i < 7; i++) {
        nuevo_pcb->ME[i] = 0;
        nuevo_pcb->MT[i] = 0;
    }
    nuevo_pcb->Estado = NEW;
    
    list_add(cola_new, nuevo_pcb);
    nuevo_pcb->ME[NEW] += 1;
    
    // Manejar Cronometro para MT
    char* pid_key = string_itoa(nuevo_pcb->PID);
    t_temporal* nuevo_cronometro = temporal_create();
    dictionary_put(tiempos_por_pid, pid_key, nuevo_cronometro);
    free(pid_key);
    
    list_add(cola_procesos, nuevo_pcb);
    log_info(kernel_log, "## (<%d>) Se crea el proceso - Estado: NEW", nuevo_pcb->PID);
}

void DUMP_MEMORY();

void IO(char* nombre_io, uint16_t tiempo_a_usar);
