#include "../headers/CPUKernel.h"

// Encuentra la CPU por su fd
cpu* get_cpu_from_fd(int fd) {
    log_debug(kernel_log, "get_cpu_from_fd: esperando mutex_lista_cpus para buscar CPU por fd=%d", fd);
    pthread_mutex_lock(&mutex_lista_cpus);
    log_debug(kernel_log, "get_cpu_from_fd: bloqueando mutex_lista_cpus para buscar CPU por fd=%d", fd);

    cpu* cpu_asociada = NULL;
    for (int i = 0; i < list_size(lista_cpus); i++) {
        cpu* c = list_get(lista_cpus, i);
        if (c->fd == fd) {
            cpu_asociada = c;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_lista_cpus);

    if (!cpu_asociada) {
        log_error(kernel_log, "No se encontró CPU asociada al fd=%d", fd);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    return cpu_asociada;
}

bool cpu_por_fd_simple(void* ptr, int fd) {
    cpu* c = (cpu*) ptr;
    return c->fd == fd;
}