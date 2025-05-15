#include "../headers/CPUKernel.h"

// Encuentra la CPU por su fd y devuelve el PID del proceso que está ejecutando
uint16_t get_pid_from_cpu(int fd, op_code instruccion) {
    pthread_mutex_lock(&mutex_lista_cpus);

    // Buscar por fd e instrucción
    cpu* cpu_asociada = NULL;
    for (int i = 0; i < list_size(lista_cpus); i++) {
        cpu* c = list_get(lista_cpus, i);
        if (c->fd == fd && c->instruccion_actual == instruccion) {
            cpu_asociada = c;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_lista_cpus);

    if (!cpu_asociada) {
        log_error(kernel_log, "No se encontró CPU asociada a fd=%d con instrucción=%d", fd, instruccion);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    return cpu_asociada->pid;
}