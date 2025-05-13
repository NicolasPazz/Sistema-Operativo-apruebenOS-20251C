#include "../headers/CPUKernel.h"

uint16_t get_pid_from_cpu(int fd, op_code instruccion) {
    pthread_mutex_lock(&mutex_lista_cpus);

    // Buscar la CPU asociada al fd y a la instrucción actual
    bool cpu_por_fd_e_instruccion(void* ptr) {
        cpu* c = (cpu*) ptr;
        return c->fd == fd && c->instruccion_actual == instruccion;
    }
    cpu* cpu_asociada = list_find(lista_cpus, cpu_por_fd_e_instruccion);

    pthread_mutex_unlock(&mutex_lista_cpus);

    if (!cpu_asociada) {
        log_error(kernel_log, "No se encontró CPU asociada a fd=%d con instrucción=%d", fd, instruccion);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    return cpu_asociada->pid;
}