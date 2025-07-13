#include "../headers/planificadores.h"
#include "../headers/MEMKernel.h"

int procesos_new_rechazados = 0;

void *planificador_largo_plazo(void *arg)
{
    pthread_mutex_lock(&mutex_planificador_lp);
    while (1)
    {
        log_debug(kernel_log, "planificador_largo_plazo: Semaforo a NEW disminuido");
        sem_wait(&sem_proceso_a_new);
        pthread_mutex_lock(&mutex_cola_susp_ready);

        if (list_is_empty(cola_susp_ready))
        {
            pthread_mutex_lock(&mutex_cola_new);
            t_pcb *pcb;

            if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0)
            {
                if (list_size(cola_new) == 1)
                {   
                    log_trace(kernel_log, "planificador_largo_plazo: Cola NEW con un solo proceso, eligiendo por FIFO");
                    pcb = elegir_por_fifo(cola_new);
                }
                else if (list_size(cola_new) > 1)
                {
                    aumentar_procesos_rechazados();
                    pthread_mutex_unlock(&mutex_cola_new);
                    pthread_mutex_unlock(&mutex_cola_susp_ready);
                    continue;
                }
            }
            else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0 && list_size(cola_new) == 1)
            {
                pcb = elegir_por_fifo(cola_new);
            }
            else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0 && list_size(cola_new) > 1)
            {
                pcb = elegir_por_pmcp(cola_new);
            }
            else
            {
                log_error(kernel_log, "planificador_largo_plazo: Cola NEW vacia o algoritmo de ingreso a READY no reconocido");
                pthread_mutex_unlock(&mutex_cola_new);
                pthread_mutex_unlock(&mutex_cola_susp_ready);
                terminar_kernel();
                exit(EXIT_FAILURE);
            }

            log_trace(kernel_log, "planificador_largo_plazo: verificando pcb obtenido de la cola NEW");

            if (!pcb)
            {
                log_error(kernel_log, "planificador_largo_plazo: No se pudo obtener un PCB de la cola NEW");
                pthread_mutex_unlock(&mutex_cola_new);
                pthread_mutex_unlock(&mutex_cola_susp_ready);
                terminar_kernel();
                exit(EXIT_FAILURE);
            }

            log_trace(kernel_log, "planificador_largo_plazo: PCB con PID %d obtenido de la cola NEW", pcb->PID);

            if (!inicializar_proceso_en_memoria(pcb))
            {
                aumentar_procesos_rechazados();
                pthread_mutex_unlock(&mutex_cola_new);
                pthread_mutex_unlock(&mutex_cola_susp_ready);
                continue;
            }

            log_trace(kernel_log, "planificador_largo_plazo: PCB con PID %d inicializado en memoria", pcb->PID);

            pthread_mutex_unlock(&mutex_cola_new);
            pthread_mutex_unlock(&mutex_cola_susp_ready);
            cambiar_estado_pcb(pcb, READY);
        }
        else
        {
            aumentar_procesos_rechazados();
            pthread_mutex_unlock(&mutex_cola_susp_ready);
        }
    }
}

void *gestionar_exit(void *arg)
{
    while (1)
    {
        log_debug(kernel_log, "gestionar_exit: Semaforo a EXIT disminuido");
        sem_wait(&sem_proceso_a_exit);

        log_debug(kernel_log, "gestionar_exit: esperando mutex_cola_exit para procesar EXIT");
        pthread_mutex_lock(&mutex_cola_exit);
        log_debug(kernel_log, "gestionar_exit: bloqueando mutex_cola_exit para procesar EXIT");

        if (list_is_empty(cola_exit))
        {
            pthread_mutex_unlock(&mutex_cola_exit);
            log_error(kernel_log, "gestionar_exit: Se despertó pero no hay procesos en EXIT");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        t_pcb *pcb = elegir_por_fifo(cola_exit);

        if (!pcb)
        {
            log_error(kernel_log, "gestionar_exit: No se pudo obtener PCB desde EXIT");
            pthread_mutex_unlock(&mutex_cola_exit);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        pthread_mutex_unlock(&mutex_cola_exit);

        EXIT(pcb);

        verificar_procesos_rechazados();
    }
}

void verificar_procesos_rechazados()
{
    pthread_mutex_lock(&mutex_cola_susp_ready);
    bool resultado = true;

    while (list_size(cola_susp_ready) > 0)
    {
        t_pcb *pcb_susp;

        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0 || list_size(cola_susp_ready) == 1)
        {
            pcb_susp = elegir_por_fifo(cola_susp_ready);
        }
        else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0)
        {
            pcb_susp = elegir_por_pmcp(cola_susp_ready);
        }
        else
        {
            log_error(kernel_log, "gestionar_exit: Algoritmo de ingreso a READY no reconocido");
            pthread_mutex_unlock(&mutex_cola_susp_ready);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        if (!pcb_susp)
        {
            log_error(kernel_log, "gestionar_exit: No se pudo obtener PCB desde SUSPENDED READY");
            pthread_mutex_unlock(&mutex_cola_susp_ready);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        resultado = desuspender_proceso(pcb_susp);
        if (!resultado)
        {
            log_debug(kernel_log, "gestionar_exit: No se pudo inicializar proceso en memoria para PID %d", pcb_susp->PID);
            break;
        }
    }
    if (!resultado)
        return;

    pthread_mutex_unlock(&mutex_cola_susp_ready);

    pthread_mutex_lock(&mutex_procesos_rechazados);
    while (procesos_new_rechazados > 0)
    {
        t_pcb *pcb;
        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0 || list_size(cola_new) == 1)
        {
            pcb = elegir_por_fifo(cola_new);
        }
        else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0 && list_size(cola_new) > 1)
        {
            pcb = elegir_por_pmcp(cola_new);
        }
        else
        {
            log_error(kernel_log, "planificador_largo_plazo: Cola NEW vacia o algoritmo de ingreso a READY no reconocido");
            pthread_mutex_unlock(&mutex_cola_new);
            pthread_mutex_unlock(&mutex_procesos_rechazados);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        if (!pcb)
        {
            log_error(kernel_log, "planificador_largo_plazo: No se pudo obtener un PCB de la cola NEW");
            pthread_mutex_unlock(&mutex_cola_new);
            pthread_mutex_unlock(&mutex_procesos_rechazados);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        if (!inicializar_proceso_en_memoria(pcb))
        {
            pthread_mutex_unlock(&mutex_cola_new);
            pthread_mutex_unlock(&mutex_procesos_rechazados);
            return;
        }

        pthread_mutex_unlock(&mutex_cola_new);
        cambiar_estado_pcb(pcb, READY);
        procesos_new_rechazados--;
    }
    pthread_mutex_unlock(&mutex_procesos_rechazados);
}

// AUXILIARES

void aumentar_procesos_rechazados()
{
    pthread_mutex_lock(&mutex_procesos_rechazados);
    procesos_new_rechazados++;
    pthread_mutex_unlock(&mutex_procesos_rechazados);
}

void disminuir_procesos_rechazados()
{
    pthread_mutex_lock(&mutex_procesos_rechazados);
    if (procesos_new_rechazados > 0)
    {
        procesos_new_rechazados--;
    }
    else
    {
        log_error(kernel_log, "disminuir_procesos_rechazados: No hay procesos rechazados para disminuir");
        pthread_mutex_unlock(&mutex_procesos_rechazados);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&mutex_procesos_rechazados);
}

t_pcb *elegir_por_fifo(t_list *cola_a_utilizar)
{
    log_trace(kernel_log, "ELIGIENDO POR FIFO");

    if (list_is_empty(cola_a_utilizar))
    {
        log_error(kernel_log, "FIFO: cola_a_utilizar vacía");
        return NULL;
    }

    log_trace(kernel_log, "FIFO: seleccionando el primer PCB de la cola");
    return (t_pcb *)list_get(cola_a_utilizar, 0);
}

t_pcb *elegir_por_pmcp(t_list *cola_a_utilizar)
{
    log_trace(kernel_log, "ELIGIENDO POR PMCP (Proceso Mas Chico Primero)");

    if (list_is_empty(cola_a_utilizar))
    {
        log_error(kernel_log, "PMCP: cola_a_utilizar vacía");
        return NULL;
    }

    log_trace(kernel_log, "PMCP: seleccionando el PCB con menor tamaño de memoria");
    return (t_pcb *)list_get_minimum(cola_a_utilizar, menor_tamanio);
}

void *menor_tamanio(void *a, void *b)
{
    t_pcb *pcb_a = (t_pcb *)a;
    t_pcb *pcb_b = (t_pcb *)b;
    return pcb_a->tamanio_memoria <= pcb_b->tamanio_memoria ? pcb_a : pcb_b;
}