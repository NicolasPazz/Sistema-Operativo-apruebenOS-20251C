#include "../headers/CPUKernel.h"

// Encuentra la CPU por su fd
cpu *get_cpu_from_fd(int fd)
{
    log_debug(kernel_log, "get_cpu_from_fd: esperando mutex_lista_cpus para buscar CPU por fd=%d", fd);
    pthread_mutex_lock(&mutex_lista_cpus);
    log_debug(kernel_log, "get_cpu_from_fd: bloqueando mutex_lista_cpus para buscar CPU por fd=%d", fd);

    cpu *cpu_asociada = NULL;
    for (int i = 0; i < list_size(lista_cpus); i++)
    {
        cpu *c = list_get(lista_cpus, i);
        if (c->fd == fd)
        {
            cpu_asociada = c;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_lista_cpus);

    if (!cpu_asociada)
    {
        log_error(kernel_log, "No se encontró CPU asociada al fd=%d", fd);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    return cpu_asociada;
}

cpu *buscar_y_remover_cpu_por_fd(int fd)
{
    log_debug(kernel_log, "buscar_y_remover_cpu_por_fd: esperando mutex_lista_cpus para buscar y remover CPU por fd=%d", fd);
    pthread_mutex_lock(&mutex_lista_cpus);
    log_debug(kernel_log, "buscar_y_remover_cpu_por_fd: bloqueando mutex_lista_cpus para buscar y remover CPU por fd=%d", fd);
    if (!lista_cpus)
    {
        log_error(kernel_log, "buscar_y_remover_cpu_por_fd: lista_cpus es NULL");
        return NULL;
    }

    // Buscar y remover la CPU por file descriptor
    for (int i = 0; i < list_size(lista_cpus); i++)
    {
        cpu *c = list_get(lista_cpus, i);
        if (c && c->fd == fd)
        {
            cpu *cpu_removida = list_remove(lista_cpus, i);
            pthread_mutex_unlock(&mutex_lista_cpus);
            return cpu_removida;
        }
    }
    pthread_mutex_unlock(&mutex_lista_cpus);

    // No se encontró la CPU
    log_debug(kernel_log, "buscar_y_remover_cpu_por_fd: No se encontró CPU con fd %d", fd);
    return NULL;
}

bool cpu_por_fd_simple(void *ptr, int fd)
{
    cpu *c = (cpu *)ptr;
    return c->fd == fd;
}

int obtener_fd_interrupt(int id_cpu)
{
    // Buscar el fd de la CPU por su ID
    for (int i = 0; i < list_size(lista_cpus); i++)
    {
        cpu *c = list_get(lista_cpus, i);
        if (c->id == id_cpu && c->tipo_conexion == CPU_INTERRUPT)
        {
            return c->fd;
        }
    }
    log_error(kernel_log, "obtener_fd_interrupt: No se encontró CPU con ID %d", id_cpu);
    terminar_kernel();
    exit(EXIT_FAILURE);
}

void liberar_cpu(cpu *cpu_a_eliminar)
{
    // Limpiar PID de la CPU asociada
    log_debug(kernel_log, "esperando mutex_lista_cpus para limpiar PID de la CPU (fd=%d)", cpu_a_eliminar->fd);
    pthread_mutex_lock(&mutex_lista_cpus);
    log_debug(kernel_log, "bloqueando mutex_lista_cpus para limpiar PID de la CPU (fd=%d)", cpu_a_eliminar->fd);

    cpu_a_eliminar->pid = -1;                // Limpiar PID de la CPU
    cpu_a_eliminar->instruccion_actual = -1; // Limpiar instrucción actual
    pthread_mutex_unlock(&mutex_lista_cpus);

    // Liberar CPU para que el planificador pueda usarla
    sem_post(&sem_cpu_disponible);
    log_debug(kernel_log, "Semáforo CPU DISPONIBLE aumentado por CPU liberada (fd=%d)", cpu_a_eliminar->fd);
}