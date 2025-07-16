#include "../headers/planificadores.h"

void *planificador_corto_plazo(void *arg)
{
    log_debug(kernel_log, "=== PLANIFICADOR CP INICIADO ===");

    while (1)
    {
        sem_wait(&sem_planificador_cp);
        pthread_mutex_lock(&mutex_cola_ready);
        if (list_is_empty(cola_ready))
        {
            log_trace(kernel_log, "[PLANI CP] Cola READY vacía, esperando nuevos procesos");
            pthread_mutex_unlock(&mutex_cola_ready);
            continue;
        }

        t_pcb *proceso_a_ejecutar;

        pthread_mutex_lock(&mutex_lista_cpus);
        mostrar_colas_estados();
        if (strcmp(ALGORITMO_CORTO_PLAZO, "FIFO") == 0)
        {
            if (cpu_libre)
            {
                log_trace(kernel_log, "[PLANI CP] [FIFO] Hay CPU disponible, eligiendo proceso");
                proceso_a_ejecutar = elegir_por_fifo(cola_ready);
            }
            else
            {
                log_trace(kernel_log, "[PLANI CP] [FIFO] No hay CPU disponible");
                pthread_mutex_unlock(&mutex_lista_cpus);
                pthread_mutex_unlock(&mutex_cola_ready);
                continue;
            }
        }
        else if (strcmp(ALGORITMO_CORTO_PLAZO, "SJF") == 0)
        {
            if (cpu_libre)
            {
                log_trace(kernel_log, "[PLANI CP] [SJF] Hay CPU disponible, eligiendo proceso");
                proceso_a_ejecutar = elegir_por_sjf();
            }
            else
            {
                log_trace(kernel_log, "[PLANI CP] [SJF] No hay CPU disponible");
                pthread_mutex_unlock(&mutex_lista_cpus);
                pthread_mutex_unlock(&mutex_cola_ready);
                continue;
            }
        }
        else if (strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0)
        {
            cpu *cpu_mayor_rafaga;
            if (cpu_libre)
            {
                log_trace(kernel_log, "[PLANI CP] [SRT] Hay CPU disponible, eligiendo proceso");
                proceso_a_ejecutar = elegir_por_srt(cola_ready);
            }
            else if ((cpu_mayor_rafaga = hay_cpu_rafaga_restante_mayor()) != NULL)
            {
                log_trace(kernel_log, "[PLANI CP] [SRT] Hay CPU con rafaga restante mayor, eligiendo proceso");
                interrupt(cpu_mayor_rafaga);
                pthread_mutex_unlock(&mutex_lista_cpus);
                pthread_mutex_unlock(&mutex_cola_ready);
                continue;
            }
            else
            {
                log_trace(kernel_log, "[PLANI CP] [SRT] No hay CPU disponible ni con rafaga restante mayor");
                pthread_mutex_unlock(&mutex_lista_cpus);
                pthread_mutex_unlock(&mutex_cola_ready);
                continue;
            }
        }
        else
        {
            log_error(kernel_log, "[PLANI CP] Algoritmo no reconocido");
            pthread_mutex_unlock(&mutex_cola_ready);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        if (!proceso_a_ejecutar)
        {
            log_error(kernel_log, "[PLANI CP] No se pudo elegir un proceso para ejecutar");
            pthread_mutex_unlock(&mutex_lista_cpus);
            pthread_mutex_unlock(&mutex_cola_ready);
            terminar_kernel();
            exit(EXIT_FAILURE);   
        }

        pthread_mutex_unlock(&mutex_lista_cpus);
        pthread_mutex_unlock(&mutex_cola_ready);

        dispatch(proceso_a_ejecutar);
    }
}

void dispatch(t_pcb *proceso_a_ejecutar)
{
    log_trace(kernel_log, "=== DISPATCH INICIADO PARA PID %d ===", proceso_a_ejecutar->PID);

    pthread_mutex_lock(&mutex_lista_cpus);
    cpu *cpu_disponible = proxima_cpu_libre();

    if (!cpu_libre || !cpu_disponible)
    {
        log_error(kernel_log, "[DISPATCH] No hay CPU libre para ejecutar PID %d", proceso_a_ejecutar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    ejecutar_proceso(cpu_disponible, proceso_a_ejecutar);
    pthread_mutex_unlock(&mutex_lista_cpus);

    log_trace(kernel_log, "[DISPATCH] Proceso %d despachado a CPU %d (PC=%d)", proceso_a_ejecutar->PID, cpu_disponible->id, proceso_a_ejecutar->PC);
}

// AUXILIARES

t_pcb *elegir_por_sjf()
{
    log_trace(kernel_log, "PLANIFICANDO SJF (Shortest Job First)");

    if (list_is_empty(cola_ready))
    {
        log_error(kernel_log, "[SJF] cola_ready vacía");
        return NULL;
    }

    int cantidad_procesos = list_size(cola_ready);

    log_trace(kernel_log, "[SJF] buscando entre %d procesos con menor rafaga en cola_ready", cantidad_procesos);
    for (int i = 0; i < cantidad_procesos; i++)
    {
        mostrar_pcb((t_pcb *)list_get(cola_ready, i));
    }

    t_pcb *seleccionado = (t_pcb *)list_get_minimum(cola_ready, menor_rafaga);

    if (seleccionado)
    {
        log_trace(kernel_log, "[SJF] Proceso elegido PID=%d con estimación=%.3f", seleccionado->PID, seleccionado->estimacion_rafaga);
    }
    else
    {
        log_error(kernel_log, "[SJF] No se pudo seleccionar un proceso");
        return NULL;
    }

    return seleccionado;
}

void *menor_rafaga(void *a, void *b)
{
    t_pcb *pcb_a = (t_pcb *)a;
    t_pcb *pcb_b = (t_pcb *)b;

    if (!a || !b)
    {
        log_error(kernel_log, "[SJF] Error al comparar procesos: uno de los parámetros es NULL");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "[SJF] Comparando procesos:");
    log_trace(kernel_log, "  • PID %d - Estado: %s - Ráfaga estimada: %.3f ms", pcb_a->PID, estado_to_string(pcb_a->Estado), pcb_a->estimacion_rafaga);
    log_trace(kernel_log, "  • PID %d - Estado: %s - Ráfaga estimada: %.3f ms", pcb_b->PID, estado_to_string(pcb_b->Estado), pcb_b->estimacion_rafaga);

    if (pcb_a->estimacion_rafaga < pcb_b->estimacion_rafaga)
    {
        log_trace(kernel_log, "[SJF] PID %d tiene menor estimación (%.3f ms) que PID %d (%.3f ms)", pcb_a->PID, pcb_a->estimacion_rafaga, pcb_b->PID, pcb_b->estimacion_rafaga);
        return (t_pcb *)pcb_a;
    }

    if (pcb_b->estimacion_rafaga < pcb_a->estimacion_rafaga)
    {
        log_trace(kernel_log, "[SJF] PID %d tiene menor estimación (%.3f ms) que PID %d (%.3f ms)", pcb_b->PID, pcb_b->estimacion_rafaga, pcb_a->PID, pcb_a->estimacion_rafaga);
        return (t_pcb *)pcb_b;
    }

    // En caso de empate, devolver el primero que llegó (FIFO)
    log_trace(kernel_log, "[SJF] Empate de estimación entre PID %d y PID %d. Se elige FIFO (PID %d)", pcb_a->PID, pcb_b->PID, pcb_a->PID);
    return (t_pcb *)pcb_a;
}

t_pcb *elegir_por_srt(t_list *cola_a_evaluar)
{
    log_trace(kernel_log, "[SRT] PLANIFICANDO SRT (Shortest Remaining Time)");

    if (list_is_empty(cola_a_evaluar))
    {
        log_error(kernel_log, "[SRT] cola_a_evaluar vacía");
        return NULL;
    }

    int cantidad_procesos = list_size(cola_a_evaluar);

    log_trace(kernel_log, "[SRT] buscando entre %d procesos con menor rafaga restante en cola_a_evaluar", cantidad_procesos);
    for (int i = 0; i < cantidad_procesos; i++)
    {
        mostrar_pcb((t_pcb *)list_get(cola_a_evaluar, i));
    }

    t_pcb *seleccionado = (t_pcb *)list_get_minimum(cola_a_evaluar, menor_rafaga_restante);

    if (seleccionado)
    {
        log_trace(kernel_log, "[SRT] Proceso elegido PID=%d con estimación=%.3f", seleccionado->PID, seleccionado->estimacion_rafaga);
    }
    else
    {
        log_error(kernel_log, "[SRT] No se pudo seleccionar un proceso");
        return NULL;
    }

    return seleccionado;
}

void *menor_rafaga_restante(void *a, void *b)
{
    t_pcb *pcb_a = (t_pcb *)a;
    t_pcb *pcb_b = (t_pcb *)b;

    if (!a || !b)
    {
        log_error(kernel_log, "[SRT] Error al comparar procesos: uno de los parámetros es NULL");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    double restante_a;
    double restante_b;
    double ahora = get_time();

    if (pcb_a->tiempo_inicio_exec > 0)
    {
        restante_a = pcb_a->estimacion_rafaga - (ahora - pcb_a->tiempo_inicio_exec);
    }
    else
    {
        restante_a = pcb_a->estimacion_rafaga;
    }

    if (pcb_b->tiempo_inicio_exec > 0)
    {
        restante_b = pcb_b->estimacion_rafaga - (ahora - pcb_b->tiempo_inicio_exec);
    }
    else
    {
        restante_b = pcb_b->estimacion_rafaga;
    }

    log_trace(kernel_log, "[SRT] Comparando procesos:");
    log_trace(kernel_log, "  • PID %d - Estado: %s - Ráfaga restante: %.3f ms", pcb_a->PID, estado_to_string(pcb_a->Estado), restante_a);
    log_trace(kernel_log, "  • PID %d - Estado: %s - Ráfaga restante: %.3f ms", pcb_b->PID, estado_to_string(pcb_b->Estado), restante_b);

    if (restante_a < restante_b)
    {
        log_trace(kernel_log, "[SRT] PID %d tiene menor rafaga restante (%.3f ms) que PID %d (%.3f ms)", pcb_a->PID, restante_a, pcb_b->PID, restante_b);
        return (t_pcb *)pcb_a;
    }
    if (restante_b < restante_a)
    {
        log_trace(kernel_log, "[SRT] PID %d tiene menor rafaga restante (%.3f ms) que PID %d (%.3f ms)", pcb_b->PID, restante_b, pcb_a->PID, restante_a);
        return (t_pcb *)pcb_b;
    }

    // En caso de empate, devolver el primero que llegó (FIFO)
    log_trace(kernel_log, "[SRT] Empate de rafaga restante entre PID %d y PID %d. Se elige FIFO (PID %d)", pcb_a->PID, pcb_b->PID, pcb_a->PID);
    return (t_pcb *)pcb_a;
}