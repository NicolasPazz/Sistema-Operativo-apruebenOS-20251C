#include "../headers/CPUKernel.h"

// Encuentra la CPU por su fd
cpu *get_cpu_from_fd(int fd)
{
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
    log_trace(kernel_log, "buscar_y_remover_cpu_por_fd: No se encontró CPU con fd %d", fd);
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
    log_trace(kernel_log, "esperando mutex_lista_cpus para limpiar PID de la CPU (fd=%d)", cpu_a_eliminar->fd);
    pthread_mutex_lock(&mutex_lista_cpus);
    log_trace(kernel_log, "bloqueando mutex_lista_cpus para limpiar PID de la CPU (fd=%d)", cpu_a_eliminar->fd);

    cpu_a_eliminar->pid = -1;                // Limpiar PID de la CPU
    cpu_a_eliminar->instruccion_actual = -1; // Limpiar instrucción actual
    cpu_libre++;                             // Aumentar contador de CPUs libres
    pthread_mutex_unlock(&mutex_lista_cpus);

    // Liberar CPU para que el planificador pueda usarla
    sem_post(&sem_planificador_cp);
    log_trace(kernel_log, "[PLANI CP] Replanificación solicitada por liberación de CPU (fd=%d, ID=%d)", cpu_a_eliminar->fd, cpu_a_eliminar->id);
}

cpu *proxima_cpu_libre()
{
    cpu *cpu_disponible = NULL;
    int total_cpus = list_size(lista_cpus);
    int cpus_dispatch = 0;
    int cpus_libres = 0;

    for (int i = 0; i < total_cpus; i++)
    {
        cpu *c = list_get(lista_cpus, i);
        if (c->tipo_conexion == CPU_DISPATCH)
        {
            cpus_dispatch++;
            if (c->pid == -1)
            {
                cpus_libres++;
                if (cpu_disponible == NULL)
                {
                    cpu_disponible = c;
                    log_trace(kernel_log, "Proxima CPU libre encontrada: ID=%d, fd=%d", c->id, c->fd);
                }
            }
        }
        log_trace(kernel_log, "CPU %d - tipo=%d, pid=%d, fd=%d, estado=%s", c->id, c->tipo_conexion, c->pid, c->fd, c->tipo_conexion == CPU_DISPATCH ? (c->pid == -1 ? "LIBRE" : "OCUPADA") : "NO-DISPATCH");
    }

    log_trace(kernel_log, "Total CPUs=%d, CPUs DISPATCH=%d, CPUs libres=%d", total_cpus, cpus_dispatch, cpus_libres);

    return cpu_disponible;
}

void ejecutar_proceso(cpu *cpu_disponible, t_pcb *proceso_a_ejecutar)
{
    // Marcar CPU como ocupada y guardar PID
    cpu_disponible->pid = proceso_a_ejecutar->PID;
    cpu_disponible->instruccion_actual = EXEC_OP;
    cpu_libre--;

    cambiar_estado_pcb_mutex(proceso_a_ejecutar, EXEC);

    t_paquete *paquete = crear_paquete_op(EXEC_OP);
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PC);
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PID);

    enviar_paquete(paquete, cpu_disponible->fd);
    eliminar_paquete(paquete);
}

cpu *hay_cpu_rafaga_restante_mayor()
{
    t_pcb *candidato_ready = elegir_por_srt(cola_ready);

    if (!candidato_ready)
    {
        log_error(kernel_log, "hay_cpu_rafaga_restante_mayor: No se encontró candidato en cola READY");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    if (candidato_ready->tiempo_inicio_exec > 0)
    {
        log_error(kernel_log, "hay_cpu_rafaga_restante_mayor: Proceso %d en cola READY no tiene tiempo de inicio de ejecución válido", candidato_ready->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    double rafaga_ready_min = candidato_ready->estimacion_rafaga;

    pthread_mutex_lock(&mutex_cola_running);

    if (list_is_empty(cola_running))
    {
        log_debug(kernel_log, "hay_cpu_rafaga_restante_mayor: Cola RUNNING está vacía (no hay procesos ejecutándose ni cpu_libre)");
        pthread_mutex_unlock(&mutex_cola_running);
        return NULL;
    }

    int cantidad_exec = list_size(cola_running);

    t_pcb *candidato_exec = NULL;
    double rafaga_exec_max = -1;
    double ahora = get_time();

    for (int i = 0; i < cantidad_exec; i++)
    {
        t_pcb *candidato_exec_actual = (t_pcb *)list_get(cola_running, i);

        if (candidato_exec_actual->tiempo_inicio_exec > 0)
        {
            double restante_exec = candidato_exec_actual->estimacion_rafaga - (ahora - candidato_exec_actual->tiempo_inicio_exec);
            if (restante_exec > rafaga_exec_max)
            {
                rafaga_exec_max = restante_exec;
                candidato_exec = candidato_exec_actual;
            }
        }
        else
        {
            log_error(kernel_log, "hay_cpu_rafaga_restante_mayor: Proceso %d en cola RUNNING no tiene tiempo de inicio de ejecución válido", candidato_exec_actual->PID);
            pthread_mutex_unlock(&mutex_cola_running);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
    }

    if (rafaga_ready_min < rafaga_exec_max)
    {
        log_trace(kernel_log, "hay_cpu_rafaga_restante_mayor: Proceso READY PID=%d tiene rafaga restante menor que el proceso RUNNING PID=%d", candidato_ready->PID, candidato_exec->PID);
        pthread_mutex_unlock(&mutex_cola_running);
        return (cpu *)get_cpu_dispatch_by_pid(candidato_exec->PID);
    }

    pthread_mutex_unlock(&mutex_cola_running);
    return NULL;
}

cpu *get_cpu_dispatch_by_pid(int pid)
{
    cpu *cpu_asociada = NULL;
    int cantidad_cpus = list_size(lista_cpus);
    for (int i = 0; i < cantidad_cpus; i++)
    {
        cpu *c = list_get(lista_cpus, i);
        if (c->tipo_conexion == CPU_DISPATCH && c->pid == pid)
        {
            cpu_asociada = c;
            break;
        }
    }

    if (!cpu_asociada)
    {
        log_error(kernel_log, "No se encontró CPU asociada al PID=%d", pid);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    return cpu_asociada;
}

void interrumpir_ejecucion(cpu *cpu_a_desalojar)
{
    pthread_mutex_lock(&mutex_lista_cpus);

    int fd_interrupt = obtener_fd_interrupt(cpu_a_desalojar->id);
    if (fd_interrupt < 0)
    {
        log_error(kernel_log, VERDE("[INTERRUPT] No se encontró fd_interrupt para CPU %d"), cpu_a_desalojar->id);
        pthread_mutex_unlock(&mutex_lista_cpus);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    int pid_exec = cpu_a_desalojar->pid;

    pthread_mutex_unlock(&mutex_lista_cpus);

    log_trace(kernel_log, VERDE("[INTERRUPT] Enviando interrupción a CPU %d (fd=%d)"), cpu_a_desalojar->id, fd_interrupt);

    t_paquete *paquete = crear_paquete_op(INTERRUPCION_OP);
    agregar_entero_a_paquete(paquete, pid_exec);
    enviar_paquete(paquete, fd_interrupt);
    eliminar_paquete(paquete);

    int respuesta = recibir_operacion(fd_interrupt);

    switch (respuesta)
    {
    case OK:
        log_trace(kernel_log, VERDE("[INTERRUPT] CPU %d respondió OK"), cpu_a_desalojar->id);

        t_list *contenido = recibir_contenido_paquete(fd_interrupt);
        if (!contenido)
        {
            log_error(kernel_log, "[INTERRUPT] El contenido recibido es NULL");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        log_trace(kernel_log, "[INTERRUPT] Cantidad de elementos en contenido recibido: %d", list_size(contenido));

        if (list_size(contenido) < 2)
        {
            log_error(kernel_log, "[INTERRUPT] Error en buffer recibido de CPU");
            list_destroy_and_destroy_elements(contenido, free);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        int pid_recibido = *(int *)list_get(contenido, 0);
        int nuevo_pc = *(int *)list_get(contenido, 1);
        list_destroy_and_destroy_elements(contenido, free);

        if (pid_recibido != pid_exec)
        {
            log_error(kernel_log, "[INTERRUPT] PID recibido (%d) no coincide con PID esperado (%d)", pid_recibido, pid_exec);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        t_pcb *pcb = buscar_pcb(pid_recibido);

        log_info(kernel_log, MAGENTA("## (%d) - Desalojado por SJF/SRT"), pid_recibido);
        log_trace(kernel_log, "[INTERRUPT] Actualizando PCB PID=%d con nuevo PC=%d", pid_recibido, nuevo_pc);

        pcb->PC = nuevo_pc;
        cambiar_estado_pcb_mutex(pcb, READY);
        liberar_cpu(cpu_a_desalojar);
        log_trace(kernel_log, "[INTERRUPT] CPU %d liberada", cpu_a_desalojar->id);
        break;
    case ERROR:
        log_trace(kernel_log, VERDE("[INTERRUPT] CPU %d respondió con ERROR"), cpu_a_desalojar->id);
        break;
    default:
        log_error(kernel_log, "[INTERRUPT] No se pudo recibir respuesta de CPU %d", cpu_a_desalojar->id);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
}

int get_exec_pid_from_id(int id)
{
    cpu *cpu_asociada = NULL;
    int cantidad_cpus = list_size(lista_cpus);
    for (int i = 0; i < cantidad_cpus; i++)
    {
        cpu *c = list_get(lista_cpus, i);
        if (c->tipo_conexion == CPU_DISPATCH && c->id == id)
        {
            cpu_asociada = c;
            break;
        }
    }
    if (!cpu_asociada)
    {
        log_error(kernel_log, "get_exec_pid_from_id: No se encontró CPU asociada al ID=%d", id);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    return cpu_asociada->pid;
}