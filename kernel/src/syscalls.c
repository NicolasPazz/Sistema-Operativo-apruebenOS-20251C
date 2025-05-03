#include "../headers/syscalls.h"
#include "../headers/planificadores.h"
//#define ESTIMACION_INICIAL 1

t_temporal* tiempo_estado_actual;

/////////////////////////////// Funciones ///////////////////////////////
void INIT_PROC(char* nombre_archivo, uint16_t tam_memoria) {
    t_pcb* nuevo_pcb = malloc(sizeof(t_pcb));
    if (nuevo_pcb == NULL) {
        log_error(kernel_log, "INIT_PROC: Error al reservar memoria para nuevo PCB");
        return;
    }

    nuevo_pcb->PID = list_size(cola_procesos);  // Asignacion secuencial de PID
    nuevo_pcb->PC = 0;

    for (int i = 0; i < 7; i++) {
        nuevo_pcb->ME[i] = 0;
        nuevo_pcb->MT[i] = 0;
    }

    nuevo_pcb->Estado = NEW;
    nuevo_pcb->tiempo_inicio_exec = 0;
    nuevo_pcb->estimacion_rafaga = atof(ESTIMACION_INICIAL);

    // Asignar path y tamanio
    nuevo_pcb->path = strdup(nombre_archivo);
    nuevo_pcb->tamanio_memoria = tam_memoria;

    if (nuevo_pcb->path == NULL) {
        log_error(kernel_log, "INIT_PROC: Error al copiar el path del archivo");
        free(nuevo_pcb);
        return;
    }

    if (nuevo_pcb->tamanio_memoria < 0) {
        log_error(kernel_log, "INIT_PROC: Error al copiar el tamanio de memoria");
        free(nuevo_pcb);
        return;
    }

    // Agregar a NEW
    pthread_mutex_lock(&mutex_cola_new);
    list_add(cola_new, nuevo_pcb);
    nuevo_pcb->ME[NEW]++;
    pthread_mutex_unlock(&mutex_cola_new);

    // Notificar al planificador LP
    
    // Cronometro de metricas de tiempo
    char* pid_key = string_itoa(nuevo_pcb->PID);
    t_temporal* nuevo_cronometro = temporal_create();
    dictionary_put(tiempos_por_pid, pid_key, nuevo_cronometro);
    free(pid_key);

    // Agregar a cola general de procesos
    list_add(cola_procesos, nuevo_pcb);
    log_info(kernel_log, "## (<%d>) Se crea el proceso - Estado: NEW", nuevo_pcb->PID);
}

void DUMP_MEMORY();

void IO(char* nombre_io, uint16_t tiempo_a_usar);
