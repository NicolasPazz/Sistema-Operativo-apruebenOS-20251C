#include "../headers/planificadores.h"
#include "../headers/MEMKernel.h"

int procesos_new_rechazados = 0;

void *planificador_largo_plazo(void *arg)
{
    pthread_mutex_lock(&mutex_planificador_lp);
    log_debug(kernel_log, "=== PLANIFICADOR LP INICIADO ===");
    while (1)
    {
        log_trace(kernel_log, "[PLANI LP] Semaforo a NEW disminuido");
        sem_wait(&sem_proceso_a_new);

        log_trace(kernel_log, "[PLANI LP] esperando mutex_cola_susp_ready para procesar SUSPENDED READY");
        pthread_mutex_lock(&mutex_cola_susp_ready);
        log_trace(kernel_log, "[PLANI LP] bloqueando mutex_cola_susp_ready para procesar SUSPENDED READY");

        bool susp_vacia = list_is_empty(cola_susp_ready);
        pthread_mutex_unlock(&mutex_cola_susp_ready);

        if (!susp_vacia)
        {
            log_trace(kernel_log, "[PLANI LP] Cola SUSPENDED READY no está vacía, se debera esperar a que se libere un proceso");
            aumentar_procesos_rechazados();
            continue;
        }

        log_trace(kernel_log, "[PLANI LP] esperando mutex_cola_new para procesar NEW");
        pthread_mutex_lock(&mutex_cola_new);
        log_trace(kernel_log, "[PLANI LP] bloqueando mutex_cola_new para procesar NEW");

        t_pcb *pcb = NULL;
        int cant_new = list_size(cola_new);

        if (cant_new == 0)
        {
            log_error(kernel_log, "[PLANI LP] Cola NEW vacia");
            pthread_mutex_unlock(&mutex_cola_new);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&mutex_inicializacion_procesos);
        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0)
        {
            if (hay_rechazados_esperando())
            {
                log_debug(kernel_log, "[PLANI LP] Cola NEW con procesos rechazados, rechazando nuevos procesos");
                aumentar_procesos_rechazados();
                pthread_mutex_unlock(&mutex_inicializacion_procesos);
                pthread_mutex_unlock(&mutex_cola_new);
                continue;
            }
            else if (cant_new >= 1)
            {
                log_debug(kernel_log, "[PLANI LP] Cola NEW sin procesos rechazados, eligiendo por FIFO");
                pcb = elegir_por_fifo(cola_new);
            }
        }
        else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0)
        {
            if (cant_new == 1)
            {
                log_debug(kernel_log, "[PLANI LP] Cola NEW con un solo proceso, eligiendo por FIFO");
                pcb = elegir_por_fifo(cola_new);
            }
            else if (cant_new > 1)
            {
                log_debug(kernel_log, "[PLANI LP] Cola NEW con varios procesos, eligiendo por PMCP");
                pcb = ultimo_si_es_menor();
                if (pcb)
                {
                    log_debug(kernel_log, "[PLANI LP] Último proceso ingresado a NEW con PID %d es el de menor tamaño", pcb->PID);
                }
                else
                {
                    log_debug(kernel_log, "[PLANI LP] El ultimo proceso ingresado a NEW no es menor que los demás, rechazando");
                    aumentar_procesos_rechazados();
                    pthread_mutex_unlock(&mutex_inicializacion_procesos);
                    pthread_mutex_unlock(&mutex_cola_new);
                    continue;
                }
            }
        }
        else
        {
            log_error(kernel_log, "[PLANI LP] Cola NEW vacia o algoritmo de ingreso a READY no reconocido");
            pthread_mutex_unlock(&mutex_inicializacion_procesos);
            pthread_mutex_unlock(&mutex_cola_new);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        if (!pcb || pcb == NULL)
        {
            log_error(kernel_log, "[PLANI LP] No se pudo obtener un PCB de la cola NEW");
            pthread_mutex_unlock(&mutex_inicializacion_procesos);
            pthread_mutex_unlock(&mutex_cola_new);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        log_debug(kernel_log, "[PLANI LP] PCB con PID %d obtenido de la cola NEW", pcb->PID);

        pthread_mutex_unlock(&mutex_cola_new);
        
        if (!inicializar_proceso_en_memoria(pcb))
        {
            aumentar_procesos_rechazados();
            pthread_mutex_unlock(&mutex_inicializacion_procesos);
            continue;
        }

        cambiar_estado_pcb_mutex(pcb, READY);
        pthread_mutex_unlock(&mutex_inicializacion_procesos);
    }
}

void *gestionar_exit(void *arg)
{
    while (1)
    {
        log_trace(kernel_log, "[PLANI LP] [EXIT] Semaforo a EXIT disminuido");
        sem_wait(&sem_proceso_a_exit);

        log_trace(kernel_log, "[PLANI LP] [EXIT] esperando mutex_cola_exit para procesar EXIT");
        pthread_mutex_lock(&mutex_cola_exit);
        log_trace(kernel_log, "[PLANI LP] [EXIT] bloqueando mutex_cola_exit para procesar EXIT");

        if (list_is_empty(cola_exit))
        {
            pthread_mutex_unlock(&mutex_cola_exit);
            log_error(kernel_log, "[PLANI LP] [EXIT] Se despertó pero no hay procesos en EXIT");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        t_pcb *pcb = elegir_por_fifo(cola_exit);

        if (!pcb || pcb == NULL)
        {
            log_error(kernel_log, "[PLANI LP] [EXIT] No se pudo obtener PCB desde EXIT");
            pthread_mutex_unlock(&mutex_cola_exit);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        pthread_mutex_unlock(&mutex_cola_exit);

        EXIT(pcb);

        verificar_procesos_rechazados();
        /*
        pthread_t hilo;
        if (pthread_create(&hilo, NULL, verificar_procesos_rechazados, NULL) != 0)
        {
            log_error(kernel_log, "[PLANI LP] [EXIT] Error al crear hilo para verificar procesos rechazados");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        pthread_detach(hilo);*/
    }
}

void *verificar_procesos_rechazados()
{
    pthread_mutex_lock(&mutex_inicializacion_procesos);
    log_trace(kernel_log, "[PLANI LP] [RECHAZADOS] esperando mutex_cola_susp_ready para procesar SUSPENDED READY");
    pthread_mutex_lock(&mutex_cola_susp_ready);
    log_trace(kernel_log, "[PLANI LP] [RECHAZADOS] bloqueando mutex_cola_susp_ready para procesar SUSPENDED READY");

    bool resultado = true;

    while (!list_is_empty(cola_susp_ready))
    {
        t_pcb *pcb_susp;
        int cant_susp = list_size(cola_susp_ready);

        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0 || cant_susp == 1)
        {
            pcb_susp = elegir_por_fifo(cola_susp_ready);
        }
        else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0 && cant_susp > 1)
        {
            pcb_susp = elegir_por_pmcp(cola_susp_ready);
        }
        else
        {
            log_error(kernel_log, "[PLANI LP] [RECHAZADOS] Algoritmo de ingreso a READY no reconocido o cola SUSPENDED READY vacia");
            pthread_mutex_unlock(&mutex_cola_susp_ready);
            pthread_mutex_unlock(&mutex_inicializacion_procesos);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        if (!pcb_susp || pcb_susp == NULL)
        {
            log_error(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo obtener PCB desde SUSP READY");
            pthread_mutex_unlock(&mutex_cola_susp_ready);
            pthread_mutex_unlock(&mutex_inicializacion_procesos);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        resultado = desuspender_proceso(pcb_susp);

        if (!resultado)
        {
            log_debug(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo desuspender proceso en memoria para PID %d", pcb_susp->PID);
            pthread_mutex_unlock(&mutex_cola_susp_ready);
            pthread_mutex_unlock(&mutex_inicializacion_procesos);
            return NULL;
        }
        else
        {
            log_debug(kernel_log, "[PLANI LP] [RECHAZADOS] Proceso PID %d desuspendido correctamente", pcb_susp->PID);
            pthread_mutex_unlock(&mutex_cola_susp_ready);
            cambiar_estado_pcb_mutex(pcb_susp, READY);
            pthread_mutex_lock(&mutex_cola_susp_ready);
            //disminuir_procesos_rechazados();
        }
    }

    pthread_mutex_unlock(&mutex_cola_susp_ready);

    if (!resultado)
    {
        pthread_mutex_unlock(&mutex_inicializacion_procesos);
        return NULL;
    }

    log_trace(kernel_log, "[PLANI LP] [RECHAZADOS] esperando mutex_procesos_rechazados para procesar procesos rechazados");
    pthread_mutex_lock(&mutex_procesos_rechazados);
    log_trace(kernel_log, "[PLANI LP] [RECHAZADOS] bloqueando mutex_procesos_rechazados para procesar procesos rechazados");

    while (procesos_new_rechazados > 0)
    {
        log_trace(kernel_log, "[PLANI LP] [RECHAZADOS] esperando mutex_cola_new para procesar NEW");
        pthread_mutex_lock(&mutex_cola_new);
        log_trace(kernel_log, "[PLANI LP] [RECHAZADOS] bloqueando mutex_cola_new para procesar NEW");

        t_pcb *pcb;
        int cant_new = list_size(cola_new);

        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0 || cant_new == 1)
        {
            pcb = elegir_por_fifo(cola_new);
        }
        else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0 && cant_new > 1)
        {
            pcb = elegir_por_pmcp(cola_new);
        }
        else
        {
            log_error(kernel_log, "[PLANI LP] [RECHAZADOS] Cola NEW vacia o algoritmo de ingreso a READY no reconocido");
            pthread_mutex_unlock(&mutex_cola_new);
            pthread_mutex_unlock(&mutex_procesos_rechazados);
            pthread_mutex_unlock(&mutex_inicializacion_procesos);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        if (!pcb || pcb == NULL)
        {
            log_error(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo obtener un PCB de la cola NEW");
            pthread_mutex_unlock(&mutex_cola_new);
            pthread_mutex_unlock(&mutex_procesos_rechazados);
            pthread_mutex_unlock(&mutex_inicializacion_procesos);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        if (!inicializar_proceso_en_memoria(pcb))
        {
            log_debug(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo inicializar proceso en memoria para PID %d", pcb->PID);
            pthread_mutex_unlock(&mutex_cola_new);
            pthread_mutex_unlock(&mutex_inicializacion_procesos);
            break;
        }
        else
        {
            log_debug(kernel_log, "[PLANI LP] [RECHAZADOS] Proceso PID %d inicializado en memoria", pcb->PID);
            pthread_mutex_unlock(&mutex_cola_new);
            cambiar_estado_pcb_mutex(pcb, READY);
            procesos_new_rechazados--;
        }
    }
    pthread_mutex_unlock(&mutex_procesos_rechazados);
    pthread_mutex_unlock(&mutex_inicializacion_procesos);

    return NULL;
}

// AUXILIARES

void aumentar_procesos_rechazados()
{
    log_trace(kernel_log, "[PLANI LP] esperando mutex_procesos_rechazados para aumentar procesos rechazados");
    pthread_mutex_lock(&mutex_procesos_rechazados);
    log_trace(kernel_log, "[PLANI LP] bloqueando mutex_procesos_rechazados para aumentar procesos rechazados");
    procesos_new_rechazados++;
    log_debug(kernel_log, "[PLANI LP] Aumentando procesos rechazados a %d", procesos_new_rechazados);
    pthread_mutex_unlock(&mutex_procesos_rechazados);
}

bool hay_rechazados_esperando()
{
    log_trace(kernel_log, "[PLANI LP] esperando mutex_procesos_rechazados para verificar si hay rechazados esperando");
    pthread_mutex_lock(&mutex_procesos_rechazados);
    log_trace(kernel_log, "[PLANI LP] bloqueando mutex_procesos_rechazados para verificar si hay rechazados esperando");

    bool hay_rechazados = procesos_new_rechazados > 0;

    log_debug(kernel_log, "[PLANI LP] hay_rechazados_esperando: hay %d rechazados esperando", procesos_new_rechazados);
    pthread_mutex_unlock(&mutex_procesos_rechazados);
    return hay_rechazados;
}

void disminuir_procesos_rechazados()
{
    log_trace(kernel_log, "[PLANI LP] disminuir_procesos_rechazados: esperando mutex_procesos_rechazados para disminuir procesos rechazados");
    pthread_mutex_lock(&mutex_procesos_rechazados);
    log_trace(kernel_log, "[PLANI LP] disminuir_procesos_rechazados: bloqueando mutex_procesos_rechazados para disminuir procesos rechazados");

    if (procesos_new_rechazados > 0)
    {
        procesos_new_rechazados--;
        log_debug(kernel_log, "[PLANI LP] disminuir_procesos_rechazados: Procesos rechazados disminuidos a %d", procesos_new_rechazados);
    }
    else
    {
        log_error(kernel_log, "[PLANI LP] disminuir_procesos_rechazados: No hay procesos rechazados para disminuir");
        pthread_mutex_unlock(&mutex_procesos_rechazados);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&mutex_procesos_rechazados);
}

t_pcb *elegir_por_pmcp(t_list *cola_a_utilizar)
{
    log_trace(kernel_log, "[PLANI LP] ELIGIENDO POR PMCP (Proceso Mas Chico Primero)");

    if (list_is_empty(cola_a_utilizar))
    {
        log_error(kernel_log, "[PLANI LP] PMCP: cola_a_utilizar vacía");
        return NULL;
    }

    log_trace(kernel_log, "[PLANI LP] PMCP: seleccionando el PCB con menor tamaño de memoria");
    return (t_pcb *)list_get_minimum(cola_a_utilizar, menor_tamanio);
}

t_pcb *ultimo_proceso_new()
{
    if (list_is_empty(cola_new))
        return NULL;

    return (t_pcb *)list_get(cola_new, list_size(cola_new) - 1);
}

t_pcb *ultimo_si_es_menor()
{
    t_pcb *candidato = ultimo_proceso_new();
    if (candidato == NULL)
        return NULL;

    t_pcb *minimo = elegir_por_pmcp(cola_new);

    return (candidato == minimo && candidato->PID == minimo->PID && candidato->tamanio_memoria == minimo->tamanio_memoria) ? candidato : NULL;
}

void *menor_tamanio(void *a, void *b)
{
    t_pcb *pcb_a = (t_pcb *)a;
    t_pcb *pcb_b = (t_pcb *)b;
    return ((t_pcb *)pcb_a)->tamanio_memoria <= ((t_pcb *)pcb_b)->tamanio_memoria ? ((t_pcb *)pcb_a) : ((t_pcb *)pcb_b);
}