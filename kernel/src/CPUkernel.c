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
        LOG_TRACE(kernel_log, "No se encontró CPU asociada al fd=%d", fd);
        return NULL;
    }

    return cpu_asociada;
}

cpu *buscar_y_remover_cpu_por_fd(int fd)
{
    if (!lista_cpus)
    {
        LOG_TRACE(kernel_log, "lista_cpus es NULL");
        return NULL;
    }

    // Buscar y remover la CPU por file descriptor
    for (int i = 0; i < list_size(lista_cpus); i++)
    {
        cpu *c = list_get(lista_cpus, i);
        if (c && c->fd == fd)
        {
            cpu *cpu_removida = list_remove(lista_cpus, i);
            return cpu_removida;
        }
    }
    LOG_TRACE(kernel_log, "No se encontró CPU con fd %d", fd);
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
    LOG_TRACE(kernel_log, "obtener_fd_interrupt: No se encontró CPU con ID %d", id_cpu);
    return -1;
}

void liberar_cpu(cpu *cpu_a_eliminar)
{
    // Limpiar PID de la CPU asociada
    LOCK_CON_LOG(mutex_lista_cpus);

    cpu_a_eliminar->pid = -1;                // Limpiar PID de la CPU
    cpu_a_eliminar->instruccion_actual = -1; // Limpiar instrucción actual
    cpu_libre++;                             // Aumentar contador de CPUs libres
    UNLOCK_CON_LOG(mutex_lista_cpus);

    // Liberar CPU para que el planificador pueda usarla
    SEM_POST(sem_planificador_cp);
    LOG_TRACE(kernel_log, "[PLANI CP] Replanificación solicitada por liberación de CPU (fd=%d, ID=%d)", cpu_a_eliminar->fd, cpu_a_eliminar->id);
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
                if (!cpu_disponible)
                {
                    cpu_disponible = c;
                    LOG_TRACE(kernel_log, "Proxima CPU libre encontrada: ID=%d, fd=%d", c->id, c->fd);
                }
            }
        }
        LOG_TRACE(kernel_log, "CPU %d - tipo=%d, pid=%d, fd=%d, estado=%s", c->id, c->tipo_conexion, c->pid, c->fd, c->tipo_conexion == CPU_DISPATCH ? (c->pid == -1 ? "LIBRE" : "OCUPADA") : "NO-DISPATCH");
    }

    LOG_TRACE(kernel_log, "Total CPUs=%d, CPUs DISPATCH=%d, CPUs libres=%d", total_cpus, cpus_dispatch, cpus_libres);

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

/**
 * @brief Busca la CPU asociada al proceso en ejecución (RUNNING) con la mayor ráfaga restante,
 *        siempre y cuando exista un proceso en READY con una ráfaga estimada menor.
 *
 * Esta función implementa la lógica de comparación entre el proceso con menor ráfaga estimada en READY
 * (utilizando Shortest Remaining Time, SRT) y los procesos actualmente en ejecución (RUNNING).
 * Si algún proceso en RUNNING tiene una ráfaga restante mayor que la mínima de READY, retorna la CPU
 * asociada a ese proceso RUNNING. Si no se cumple la condición, retorna NULL.
 *
 * @return cpu* Puntero a la CPU del proceso RUNNING con mayor ráfaga restante si corresponde, o NULL en caso contrario.
 */
cpu *hay_cpu_rafaga_restante_mayor()
{
    t_pcb *candidato_ready = elegir_por_srt(cola_ready);

    if (!candidato_ready)
    {
        LOG_TRACE(kernel_log, "No se encontró candidato en cola READY");
        return NULL;
    }

    if (candidato_ready->tiempo_inicio_exec > 0)
    {
        LOG_TRACE(kernel_log, "Proceso %d en cola READY no tiene tiempo de inicio de ejecución válido", candidato_ready->PID);
        return NULL;
    }

    double rafaga_ready_min = candidato_ready->estimacion_rafaga;
    LOG_DEBUG(kernel_log, "Proceso READY PID=%d tiene rafaga mínima %.3f ms", candidato_ready->PID, rafaga_ready_min);

    LOCK_CON_LOG(mutex_cola_running);

    if (list_is_empty(cola_running))
    {
        LOG_TRACE(kernel_log, "Cola RUNNING está vacía (no hay procesos ejecutándose ni cpu_libre)");
        UNLOCK_CON_LOG(mutex_cola_running);
        return NULL;
    }

    int cantidad_exec = list_size(cola_running);

    t_pcb *candidato_exec = NULL;
    double rafaga_exec_max = -1;
    double ahora = get_time();

    for (int i = 0; i < cantidad_exec; i++)
    {
        t_pcb *candidato_exec_actual = (t_pcb *)list_get(cola_running, i);

        if (!candidato_exec_actual)
        {
            LOG_TRACE(kernel_log, "Elemento NULL encontrado en cola_running en posición %d", i);
            continue;
        }

        if (candidato_exec_actual->tiempo_inicio_exec > 0)
        {
            double restante_exec = candidato_exec_actual->estimacion_rafaga - (ahora - candidato_exec_actual->tiempo_inicio_exec);
            if (restante_exec >= rafaga_exec_max)
            {
                rafaga_exec_max = restante_exec;
                candidato_exec = candidato_exec_actual;
                LOG_DEBUG(kernel_log, "Proceso RUNNING PID=%d tiene rafaga restante %.3f ms", candidato_exec->PID, rafaga_exec_max);
            }
        }
        else
        {
            LOG_TRACE(kernel_log, "Proceso %d en cola RUNNING no tiene tiempo de inicio de ejecución válido", candidato_exec_actual->PID);
            UNLOCK_CON_LOG(mutex_cola_running);
            continue;
        }
    }

    if (!candidato_exec)
    {
        LOG_TRACE(kernel_log, "No se encontró candidato en cola RUNNING");
        UNLOCK_CON_LOG(mutex_cola_running);
        return NULL;
    }

    if (rafaga_ready_min < rafaga_exec_max)
    {
        LOG_DEBUG(kernel_log, "Proceso READY PID=%d tiene rafaga menor que el proceso RUNNING PID=%d", candidato_ready->PID, candidato_exec->PID);
        if (strcmp(archivo_pseudocodigo, "PLANI_CORTO_PLAZO") == 0)
        {
            log_info(kernel_log, NARANJA("## (%d) - Elegido con menor ráfaga estimada %.3f ms"), candidato_ready->PID, candidato_ready->estimacion_rafaga);
            log_info(kernel_log, NARANJA("## (%d) - Elegido en ejecución con mayor ráfaga restante %.3f ms"), candidato_exec->PID, candidato_exec->estimacion_rafaga);
        }

        UNLOCK_CON_LOG(mutex_cola_running);
        return (cpu *)get_cpu_dispatch_by_pid(candidato_exec->PID);
    }

    UNLOCK_CON_LOG(mutex_cola_running);
    return NULL;
}

cpu *get_cpu_dispatch_by_pid(int pid)
{
    cpu *cpu_asociada = NULL;

    if (!lista_cpus)
    {
        LOG_TRACE(kernel_log, "lista_cpus es NULL");
        return NULL;
    }

    int cantidad_cpus = list_size(lista_cpus);
    for (int i = 0; i < cantidad_cpus; i++)
    {
        cpu *c = list_get(lista_cpus, i);

        if (!c)
        {
            LOG_TRACE(kernel_log, "Elemento NULL encontrado en lista_cpus en posición %d", i);
            continue;
        }

        if (c->tipo_conexion == CPU_DISPATCH && c->pid == pid)
        {
            cpu_asociada = c;
            break;
        }
    }

    if (!cpu_asociada)
    {
        LOG_TRACE(kernel_log, "No se encontró CPU asociada al PID=%d", pid);
        return NULL;
    }
    return cpu_asociada;
}

void interrumpir_ejecucion(cpu *cpu_a_desalojar)
{
    LOCK_CON_LOG(mutex_lista_cpus);

    int fd_interrupt = obtener_fd_interrupt(cpu_a_desalojar->id);
    if (fd_interrupt < 0)
    {
        LOG_TRACE(kernel_log, VERDE("[INTERRUPT] No se encontró fd_interrupt para CPU %d"), cpu_a_desalojar->id);
        UNLOCK_CON_LOG(mutex_lista_cpus);
        return;
    }
    int pid_exec = cpu_a_desalojar->pid;

    UNLOCK_CON_LOG(mutex_lista_cpus);

    LOG_DEBUG(kernel_log, VERDE("[INTERRUPT] Enviando interrupción a CPU %d (fd=%d)"), cpu_a_desalojar->id, fd_interrupt);

    t_paquete *paquete = crear_paquete_op(INTERRUPCION_OP);
    agregar_entero_a_paquete(paquete, pid_exec);
    enviar_paquete(paquete, fd_interrupt);
    eliminar_paquete(paquete);

    int respuesta = recibir_operacion(fd_interrupt);

    switch (respuesta)
    {
    case OK:
        LOG_DEBUG(kernel_log, VERDE("[INTERRUPT] CPU %d respondió OK"), cpu_a_desalojar->id);

        t_list *contenido = recibir_contenido_paquete(fd_interrupt);
        if (!contenido)
        {
            LOG_TRACE(kernel_log, "[INTERRUPT] El contenido recibido es NULL");
            return;
        }
        LOG_TRACE(kernel_log, "[INTERRUPT] Cantidad de elementos en contenido recibido: %d", list_size(contenido));

        if (list_size(contenido) < 2)
        {
            LOG_TRACE(kernel_log, "[INTERRUPT] Error en buffer recibido de CPU");
            list_destroy_and_destroy_elements(contenido, free);
            return;
        }

        int pid_recibido = *(int *)list_get(contenido, 0);
        int nuevo_pc = *(int *)list_get(contenido, 1);
        list_destroy_and_destroy_elements(contenido, free);

        if (pid_recibido != pid_exec)
        {
            LOG_TRACE(kernel_log, "[INTERRUPT] PID recibido (%d) no coincide con PID esperado (%d)", pid_recibido, pid_exec);
            return;
        }

        t_pcb *pcb = buscar_pcb(pid_recibido);

        log_info(kernel_log, MAGENTA("## (%d) - Desalojado por SJF/SRT"), pid_recibido);
        LOG_TRACE(kernel_log, "[INTERRUPT] Actualizando PCB PID=%d con nuevo PC=%d", pid_recibido, nuevo_pc);

        pcb->PC = nuevo_pc;
        cambiar_estado_pcb_mutex_srt(pcb, READY);
        liberar_cpu(cpu_a_desalojar);
        LOG_TRACE(kernel_log, "[INTERRUPT] CPU %d liberada", cpu_a_desalojar->id);
        break;
    case ERROR:
        LOG_TRACE(kernel_log, VERDE("[INTERRUPT] CPU %d respondió con ERROR"), cpu_a_desalojar->id);
        break;
    default:
        LOG_TRACE(kernel_log, "[INTERRUPT] No se pudo recibir respuesta de CPU %d", cpu_a_desalojar->id);
        return;
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
        LOG_TRACE(kernel_log, "No se encontró CPU asociada al ID=%d", id);
        return -1;
    }

    return cpu_asociada->pid;
}