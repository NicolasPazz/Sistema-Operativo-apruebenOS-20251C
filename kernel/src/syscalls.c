#include "../headers/syscalls.h"
#include "../headers/planificadores.h"
//#define ESTIMACION_INICIAL 1

t_temporal* tiempo_estado_actual;

/////////////////////////////// Funciones ///////////////////////////////
void INIT_PROC(char* nombre_archivo, uint16_t tam_memoria) {
    t_pcb* nuevo_pcb = malloc(sizeof(t_pcb));
    if (nuevo_pcb == NULL) {
        log_error(kernel_log, "INIT_PROC: Error al reservar memoria para nuevo PCB");
        free(nuevo_pcb);
        terminar_kernel();
        exit(EXIT_FAILURE);
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
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    if (nuevo_pcb->tamanio_memoria < 0) {
        log_error(kernel_log, "INIT_PROC: Error al copiar el tamanio de memoria");
        free(nuevo_pcb);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Agregar a NEW
    pthread_mutex_lock(&mutex_cola_new);
    list_add(cola_new, nuevo_pcb);
    nuevo_pcb->ME[NEW]++;
    pthread_mutex_unlock(&mutex_cola_new);

    sem_post(&sem_proceso_a_new); // Notificar al planificador LP
    
    // Cronometro de metricas de tiempo
    char* pid_key = string_itoa(nuevo_pcb->PID);
    t_temporal* nuevo_cronometro = temporal_create();
    dictionary_put(tiempos_por_pid, pid_key, nuevo_cronometro);
    free(pid_key);

    // Agregar a cola general de procesos
    list_add(cola_procesos, nuevo_pcb);
    log_info(kernel_log, "## (<%d>) Se crea el proceso - Estado: NEW", nuevo_pcb->PID);
}

void DUMP_MEMORY(){
    
}

void IO(char* nombre_io, uint16_t tiempo_a_usar){

}

void EXIT(t_pcb* pcb_a_finalizar) {
    if (!pcb_a_finalizar) {
        log_error(kernel_log, "EXIT: PCB nulo");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Notificar a Memoria
    int cod_op = FINALIZAR_PROC_OP;
    if (send(fd_memoria, &cod_op, sizeof(int), 0) <= 0 ||
        send(fd_memoria, &pcb_a_finalizar->PID, sizeof(uint16_t), 0) <= 0) {
        log_error(kernel_log, "EXIT: Error al enviar FINALIZAR_PROC_OP a Memoria para PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    t_respuesta_memoria confirmacion;
    if (recv(fd_memoria, &confirmacion, sizeof(t_respuesta_memoria), 0) <= 0) {
        log_error(kernel_log, "EXIT: No se pudo recibir confirmación de Memoria para PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    
    if (confirmacion == OK) {
        log_debug(kernel_log, "EXIT: Memoria confirmó finalización de PID %d", pcb_a_finalizar->PID);
    } else if (confirmacion == ERROR) {
        log_error(kernel_log, "EXIT: Memoria rechazó la finalización de PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    } else {
        log_error(kernel_log, "EXIT: Respuesta desconocida de Memoria para PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Logs
    log_info(kernel_log, "## (<%d>) - Finaliza el proceso", pcb_a_finalizar->PID);
    loguear_metricas_estado(pcb_a_finalizar);

    // Eliminar de cola_exit, liberar pcb y cronometro
    pthread_mutex_lock(&mutex_cola_exit);
    list_remove_element(cola_exit, pcb_a_finalizar);
    pthread_mutex_unlock(&mutex_cola_exit);

    char* pid_key = string_itoa(pcb_a_finalizar->PID);
    dictionary_remove_and_destroy(tiempos_por_pid, pid_key, (void*) temporal_destroy);
    free(pid_key);

    free(pcb_a_finalizar->path);
    free(pcb_a_finalizar);

    // Notificar a planificador LP
    sem_post(&sem_finalizacion_de_proceso);
}
