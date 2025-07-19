#include "../headers/procesos.h"
#include <commons/collections/list.h>

/////////////////////////////// Funciones ///////////////////////////////

const char *estado_to_string(Estados estado)
{
    switch (estado)
    {
    case INIT:
        return "INIT";
    case NEW:
        return "NEW";
    case READY:
        return "READY";
    case EXEC:
        return "EXEC";
    case BLOCKED:
        return "BLOCKED";
    case SUSP_READY:
        return "SUSP_READY";
    case SUSP_BLOCKED:
        return "SUSP_BLOCKED";
    case EXIT_ESTADO:
        return "EXIT";
    default:
        LOG_ERROR(kernel_log, "estado_to_string: Estado desconocido %d", estado);
        terminar_kernel(EXIT_FAILURE);
        return "DESCONOCIDO";
    }
}

void mostrar_pcb(t_pcb *PCB)
{
    if (!PCB)
    {
        LOG_ERROR(kernel_log, "PCB es NULL");
        return;
    }

    LOCK_CON_LOG_PCB(PCB->mutex, PCB->PID);
    LOG_DEBUG(kernel_log, "-*-*-*-*-*- PCB -*-*-*-*-*-");
    LOG_DEBUG(kernel_log, "PID: %d", PCB->PID);
    LOG_DEBUG(kernel_log, "PC: %d", PCB->PC);
    mostrar_metrica("ME", PCB->ME);
    mostrar_metrica("MT", PCB->MT);
    LOG_DEBUG(kernel_log, "Estado: %s", estado_to_string(PCB->Estado));
    LOG_DEBUG(kernel_log, "Tiempo inicio exec: %.3f", PCB->tiempo_inicio_exec);
    LOG_DEBUG(kernel_log, "Rafaga estimada: %.3f", PCB->estimacion_rafaga);
    LOG_DEBUG(kernel_log, "Path: %s", PCB->path ? PCB->path : "(null)");
    LOG_DEBUG(kernel_log, "Tamanio de memoria: %d", PCB->tamanio_memoria);
    LOG_DEBUG(kernel_log, "-*-*-*-*-*-*-*-*-*-*-*-*-*-");
    UNLOCK_CON_LOG_PCB(PCB->mutex, PCB->PID);
}

void mostrar_metrica(const char *nombre, int *metrica)
{
    char buffer[256];
    int offset = snprintf(buffer, sizeof(buffer), "%s: [", nombre);

    for (int i = 0; i < 7; i++)
    {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%u", metrica[i]);
        if (i < 6)
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, ", ");
    }

    snprintf(buffer + offset, sizeof(buffer) - offset, "]");

    LOG_DEBUG(kernel_log, "%s", buffer);
}

void mostrar_colas_estados()
{
    log_info(kernel_log, "Colas -> [NEW: %d, READY: %d, EXEC: %d, BLOCK: %d, SUSP.BLOCK: %d, SUSP.READY: %d, EXIT: %d] | Procesos en total: %d", list_size(cola_new), list_size(cola_ready), list_size(cola_running), list_size(cola_blocked), list_size(cola_susp_blocked), list_size(cola_susp_ready), list_size(cola_exit), list_size(cola_procesos));
}

void cambiar_estado_pcb(t_pcb *PCB, Estados nuevo_estado_enum)
{
    if (!PCB)
    {
        LOG_ERROR(kernel_log, "PCB es NULL");
        terminar_kernel(EXIT_FAILURE);
    }

    if (!transicion_valida(PCB->Estado, nuevo_estado_enum))
    {
        LOG_ERROR(kernel_log, "Transicion no valida en el PID %d: %s → %s", PCB->PID, estado_to_string(PCB->Estado), estado_to_string(nuevo_estado_enum));
        terminar_kernel(EXIT_FAILURE);
    }

    t_list *cola_destino = obtener_cola_por_estado(nuevo_estado_enum);
    if (!cola_destino)
    {
        LOG_ERROR(kernel_log, "Error al obtener las colas correspondientes");
        terminar_kernel(EXIT_FAILURE);
    }

    if (PCB->Estado != INIT)
    {
        t_list *cola_origen = obtener_cola_por_estado(PCB->Estado);

        if (!cola_origen)
        {
            LOG_ERROR(kernel_log, "Error al obtener las colas correspondientes");

            terminar_kernel(EXIT_FAILURE);
        }

        log_info(kernel_log, AZUL("## (%u) Pasa del estado ") VERDE("%s") AZUL(" al estado ") VERDE("%s"), PCB->PID, estado_to_string(PCB->Estado), estado_to_string(nuevo_estado_enum));

        bloquear_cola_por_estado(PCB->Estado);
        list_remove_element(cola_origen, PCB);
        liberar_cola_por_estado(PCB->Estado);

        char *pid_key = string_itoa(PCB->PID);
        t_temporal *cronometro = dictionary_get(tiempos_por_pid, pid_key);

        if (cronometro)
        {
            temporal_stop(cronometro);
            int64_t tiempo = temporal_gettime(cronometro);

            PCB->MT[PCB->Estado] += (int)tiempo;
            LOG_DEBUG(kernel_log, "Se actualizo el MT en el estado %s del PID %d con %ld", estado_to_string(PCB->Estado), PCB->PID, tiempo);
            temporal_destroy(cronometro);
        }
        cronometro = temporal_create();
        dictionary_put(tiempos_por_pid, pid_key, cronometro);
        free(pid_key);

        if (nuevo_estado_enum == EXEC)
        {
            PCB->tiempo_inicio_exec = get_time();
        }
        else if (PCB->Estado == EXEC)
        {
            LOG_DEBUG(kernel_log, "estimacion rafaga anterior del PID %d: %.3f", PCB->PID, PCB->estimacion_rafaga);
            double rafaga_real = get_time() - PCB->tiempo_inicio_exec;

            LOG_DEBUG(kernel_log, "Rafaga real del PID %d: %.3f", PCB->PID, rafaga_real);
            PCB->estimacion_rafaga = ALFA * rafaga_real + (1.0 - ALFA) * PCB->estimacion_rafaga;

            LOG_DEBUG(kernel_log, "Nueva estimacion de rafaga del PID %d: %.3f", PCB->PID, PCB->estimacion_rafaga);
            PCB->tiempo_inicio_exec = -1;
        }

        if (nuevo_estado_enum == BLOCKED)
        {
            PCB->tiempo_inicio_blocked = get_time();
            iniciar_timer_suspension(PCB);
        }
        else if (PCB->Estado == BLOCKED)
        {
            PCB->tiempo_inicio_blocked = -1;
            if (PCB->timer_flag)
            {
                *(PCB->timer_flag) = false;
                PCB->timer_flag = NULL;
            }
        }

        PCB->Estado = nuevo_estado_enum;
        PCB->ME[nuevo_estado_enum] += 1;
    }
    else
    {
        LOG_DEBUG(kernel_log, "proceso en INIT recibido");
        char *pid_key = string_itoa(PCB->PID);
        if (!dictionary_get(tiempos_por_pid, pid_key))
        {
            t_temporal *nuevo_crono = temporal_create();
            dictionary_put(tiempos_por_pid, pid_key, nuevo_crono);
        }
        free(pid_key);

        // Cambiar Estado y actualizar Metricas de Estados
        PCB->Estado = nuevo_estado_enum;
        PCB->ME[nuevo_estado_enum] += 1; // Se suma 1 en las Metricas de estado del nuevo estado
        bloquear_cola_por_estado(PCB->Estado);
        list_add(cola_procesos, PCB);
        liberar_cola_por_estado(PCB->Estado);
    }

    Estados estado_viejo = PCB->Estado;
    bloquear_cola_por_estado(nuevo_estado_enum);
    list_add(cola_destino, PCB);
    liberar_cola_por_estado(nuevo_estado_enum);

    switch (nuevo_estado_enum)
    {
    case NEW:
        SEM_POST(sem_proceso_a_new);
        break;
    case READY:
        SEM_POST(sem_proceso_a_ready);
        SEM_POST(sem_planificador_cp);
        LOG_DEBUG(kernel_log, "[PLANI CP] Replanificación solicitada por proceso a READY (PID=%d)", PCB->PID);
        break;
    case EXEC:
        SEM_POST(sem_proceso_a_running);
        break;
    case BLOCKED:
        SEM_POST(sem_proceso_a_blocked);
        break;
    case SUSP_READY:
        SEM_POST(sem_proceso_a_susp_ready);
        break;
    case SUSP_BLOCKED:
        SEM_POST(sem_proceso_a_susp_blocked);
        break;
    case EXIT_ESTADO:
        SEM_POST(sem_proceso_a_exit);
        break;
    default:
        LOG_ERROR(kernel_log, "nuevo_estado_enum: Error al pasar PCB de %s a %s", estado_to_string(estado_viejo), estado_to_string(nuevo_estado_enum));
        terminar_kernel(EXIT_FAILURE);
    }
    mostrar_colas_estados();
}

void cambiar_estado_pcb_mutex(t_pcb *PCB, Estados nuevo_estado_enum)
{
    if (!PCB)
    {
        LOG_ERROR(kernel_log, "PCB es NULL");
        terminar_kernel(EXIT_FAILURE);
    }

    LOCK_CON_LOG_PCB(PCB->mutex, PCB->PID);
    cambiar_estado_pcb(PCB, nuevo_estado_enum);
    UNLOCK_CON_LOG_PCB(PCB->mutex, PCB->PID);
}

bool transicion_valida(Estados actual, Estados destino)
{
    switch (actual)
    {
    case INIT:
        return destino == NEW;
    case NEW:
        return destino == READY;
    case READY:
        return destino == EXEC;
    case EXEC:
        return destino == BLOCKED || destino == READY || destino == EXIT_ESTADO;
    case BLOCKED:
        return destino == READY || destino == SUSP_BLOCKED || destino == EXIT_ESTADO;
    case SUSP_BLOCKED:
        return destino == SUSP_READY || destino == EXIT_ESTADO;
    case SUSP_READY:
        return destino == READY;
    default:
        LOG_ERROR(kernel_log, "transicion_valida: Estado desconocido %d", actual);
        terminar_kernel(EXIT_FAILURE);
        return false;
    }
}

t_list *obtener_cola_por_estado(Estados estado)
{
    switch (estado)
    {
    case NEW:
        return cola_new;
    case READY:
        return cola_ready;
    case EXEC:
        return cola_running;
    case BLOCKED:
        return cola_blocked;
    case SUSP_READY:
        return cola_susp_ready;
    case SUSP_BLOCKED:
        return cola_susp_blocked;
    case EXIT_ESTADO:
        return cola_exit;
    default:
        LOG_ERROR(kernel_log, "obtener_cola_por_estado: Estado desconocido %d", estado);
        terminar_kernel(EXIT_FAILURE);
        return NULL;
    }
}

void bloquear_cola_por_estado(Estados estado)
{
    switch (estado)
    {
    case NEW:
        LOCK_CON_LOG(mutex_cola_new);
        break;
    case READY:
        LOCK_CON_LOG(mutex_cola_ready);
        break;
    case EXEC:
        LOCK_CON_LOG(mutex_cola_running);
        break;
    case BLOCKED:
        LOCK_CON_LOG(mutex_cola_blocked);
        break;
    case SUSP_READY:
        LOCK_CON_LOG(mutex_cola_susp_ready);
        break;
    case SUSP_BLOCKED:
        LOCK_CON_LOG(mutex_cola_susp_blocked);
        break;
    case EXIT_ESTADO:
        LOCK_CON_LOG(mutex_cola_exit);
        break;
    default:
        LOG_ERROR(kernel_log, "bloquear_cola_por_estado: Estado desconocido %d", estado);
        terminar_kernel(EXIT_FAILURE);
    }
}

void liberar_cola_por_estado(Estados estado)
{
    switch (estado)
    {
    case NEW:
        UNLOCK_CON_LOG(mutex_cola_new);
        break;
    case READY:
        UNLOCK_CON_LOG(mutex_cola_ready);
        break;
    case EXEC:
        UNLOCK_CON_LOG(mutex_cola_running);
        break;
    case BLOCKED:
        UNLOCK_CON_LOG(mutex_cola_blocked);
        break;
    case SUSP_READY:
        UNLOCK_CON_LOG(mutex_cola_susp_ready);
        break;
    case SUSP_BLOCKED:
        UNLOCK_CON_LOG(mutex_cola_susp_blocked);
        break;
    case EXIT_ESTADO:
        UNLOCK_CON_LOG(mutex_cola_exit);
        break;
    default:
        LOG_ERROR(kernel_log, "liberar_cola_por_estado: Estado desconocido %d", estado);
        terminar_kernel(EXIT_FAILURE);
    }
}

void loguear_metricas_estado(t_pcb *pcb)
{
    if (!pcb)
    {
        LOG_ERROR(kernel_log, "PCB es NULL");
        return;
    }

    log_info(kernel_log, NARANJA("## (%d) - Métricas de estado:"), pcb->PID);

    for (int i = 0; i < 7; i++)
    {
        const char *nombre_estado = estado_to_string((Estados)i);
        unsigned veces = pcb->ME[i];
        unsigned tiempo = pcb->MT[i];

        log_info(
            kernel_log,
            "    " NARANJA("%-12s") " Veces: %4u | Tiempo: " VERDE("%6u ms"),
            nombre_estado,
            veces,
            tiempo);
    }

    unsigned promedio = pcb->ME[2] > 0 ? pcb->MT[1] / pcb->ME[2] : 0;

    if (strcmp(ALGORITMO_CORTO_PLAZO, "FIFO") != 0)
    {
        log_info(kernel_log, "    " NARANJA("%-24s") " | Tiempo: " VERDE("%6u ms"), "PROMEDIO DE ESPERA", promedio);
    }
}

t_pcb *buscar_pcb(int pid)
{

    LOCK_CON_LOG(mutex_cola_procesos);

    t_pcb *resultado = NULL;

    for (int i = 0; i < list_size(cola_procesos); i++)
    {
        t_pcb *pcb = list_get(cola_procesos, i);
        if (pcb->PID == pid)
        {
            resultado = pcb;
            break;
        }
    }

    UNLOCK_CON_LOG(mutex_cola_procesos);

    if (!resultado)
    {
        LOG_ERROR(kernel_log, "buscar_pcb: No se encontró PCB para PID=%d", pid);
        terminar_kernel(EXIT_FAILURE);
    }

    return resultado;
}

t_pcb *buscar_y_remover_pcb_por_pid(t_list *cola, int pid)
{
    if (!cola)
    {
        LOG_ERROR(kernel_log, "cola es NULL");
        return NULL;
    }

    // Buscar y remover el PCB de la cola específica
    for (int i = 0; i < list_size(cola); i++)
    {
        t_pcb *pcb = list_get(cola, i);
        if (pcb && pcb->PID == pid)
        {
            return list_remove(cola, i);
        }
    }

    // No se encontró el PCB
    LOG_DEBUG(kernel_log, "buscar_y_remover_pcb_por_pid: No se encontró PCB con PID %d en la cola", pid);
    return NULL;
}

void liberar_pcb(t_pcb *pcb)
{
    if (!pcb)
    {
        LOG_ERROR(kernel_log, "PCB es NULL");
        return;
    }

    if (pcb->timer_flag)
    {
        *pcb->timer_flag = false;
        //free(pcb->timer_flag);
        pcb->timer_flag = NULL;
    }

    list_remove_element(cola_exit, pcb);

    LOCK_CON_LOG(mutex_cola_procesos);
    list_remove_element(cola_procesos, pcb);
    UNLOCK_CON_LOG(mutex_cola_procesos);

    char *pid_key = string_itoa(pcb->PID);
    dictionary_remove_and_destroy(tiempos_por_pid, pid_key, (void *)temporal_destroy);
    free(pid_key);

    if (pcb->path)
    {
        free(pcb->path);
    }
    UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);
    pthread_mutex_destroy(&pcb->mutex);
    free(pcb);
}

void verificar_procesos_restantes()
{
    LOG_DEBUG(kernel_log, "EXIT: verificando si quedan procesos en el sistema");

    int total_estimado = list_size(cola_new) + list_size(cola_ready) +
                         list_size(cola_running) + list_size(cola_blocked) +
                         list_size(cola_susp_ready) + list_size(cola_susp_blocked) +
                         list_size(cola_exit) + list_size(cola_procesos);

    if (total_estimado > 1)
        return;

    LOCK_CON_LOG(mutex_cola_new);
    int cantidad_new = list_size(cola_new);
    UNLOCK_CON_LOG(mutex_cola_new);

    LOCK_CON_LOG(mutex_cola_ready);
    int cantidad_ready = list_size(cola_ready);
    UNLOCK_CON_LOG(mutex_cola_ready);

    LOCK_CON_LOG(mutex_cola_running);
    int cantidad_running = list_size(cola_running);
    UNLOCK_CON_LOG(mutex_cola_running);

    LOCK_CON_LOG(mutex_cola_blocked);
    int cantidad_blocked = list_size(cola_blocked);
    UNLOCK_CON_LOG(mutex_cola_blocked);

    LOCK_CON_LOG(mutex_cola_susp_ready);
    int cantidad_susp_ready = list_size(cola_susp_ready);
    UNLOCK_CON_LOG(mutex_cola_susp_ready);

    LOCK_CON_LOG(mutex_cola_susp_blocked);
    int cantidad_susp_blocked = list_size(cola_susp_blocked);
    UNLOCK_CON_LOG(mutex_cola_susp_blocked);

    LOCK_CON_LOG(mutex_cola_exit);
    int cantidad_exit = list_size(cola_exit);
    UNLOCK_CON_LOG(mutex_cola_exit);

    LOCK_CON_LOG(mutex_cola_procesos);
    int cantidad_procesos = list_size(cola_procesos);
    UNLOCK_CON_LOG(mutex_cola_procesos);

    int total_procesos = cantidad_new + cantidad_ready +
                         cantidad_running + cantidad_blocked +
                         cantidad_susp_ready + cantidad_susp_blocked +
                         cantidad_exit + cantidad_procesos;

    LOG_DEBUG(kernel_log, "EXIT: Total de procesos restantes en el sistema: %d", total_procesos);

    if (total_procesos == 0)
    {
        mostrar_colas_estados();
        log_info(kernel_log, "Todos los procesos han terminado. Finalizando kernel...");
        terminar_kernel(EXIT_SUCCESS);
    }
}