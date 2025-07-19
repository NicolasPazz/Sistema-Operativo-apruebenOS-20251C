#include "../headers/planificadores.h"
#include "../headers/MEMKernel.h"

int procesos_new_rechazados = 0;

void *planificador_largo_plazo(void *arg)
{
    registrar_hilo_activo();
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    LOCK_CON_LOG(mutex_planificador_lp);
    LOG_DEBUG(kernel_log, "=== PLANIFICADOR LP INICIADO ===");
    while (kernel_debe_continuar())
    {
        SEM_WAIT(sem_proceso_a_new);

        LOCK_CON_LOG(mutex_cola_susp_ready);

        bool susp_vacia = list_is_empty(cola_susp_ready);
        UNLOCK_CON_LOG(mutex_cola_susp_ready);

        if (!susp_vacia)
        {
            LOG_DEBUG(kernel_log, "[PLANI LP] Cola SUSPENDED READY no está vacía, se debera esperar a que se libere un proceso");
            aumentar_procesos_rechazados();
            continue;
        }

        LOCK_CON_LOG(mutex_inicializacion_procesos);
        LOCK_CON_LOG(mutex_cola_new);

        t_pcb *pcb = NULL;
        int cant_new = list_size(cola_new);

        if (cant_new == 0)
        {
            LOG_ERROR(kernel_log, "[PLANI LP] Cola NEW vacia");
            UNLOCK_CON_LOG(mutex_cola_new);
            UNLOCK_CON_LOG(mutex_inicializacion_procesos);
            //terminar_kernel(EXIT_FAILURE);
            continue;
        }

        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0)
        {
            if (hay_rechazados_esperando())
            {
                LOG_DEBUG(kernel_log, "[PLANI LP] Cola NEW con procesos rechazados, rechazando nuevos procesos");
                aumentar_procesos_rechazados();
                UNLOCK_CON_LOG(mutex_cola_new);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                continue;
            }
            else if (cant_new >= 1)
            {
                LOG_DEBUG(kernel_log, "[PLANI LP] Cola NEW sin procesos rechazados, eligiendo por FIFO");
                pcb = elegir_por_fifo(cola_new);
            }
        }
        else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0)
        {
            if (cant_new == 1)
            {
                LOG_DEBUG(kernel_log, "[PLANI LP] Cola NEW con un solo proceso, eligiendo por FIFO");
                pcb = elegir_por_fifo(cola_new);
            }
            else if (cant_new > 1)
            {
                LOG_DEBUG(kernel_log, "[PLANI LP] Cola NEW con varios procesos, eligiendo por PMCP");
                pcb = ultimo_si_es_menor();
                if (pcb)
                {
                    LOG_DEBUG(kernel_log, "[PLANI LP] Último proceso ingresado a NEW con PID %d es el de menor tamaño", pcb->PID);
                }
                else
                {
                    LOG_DEBUG(kernel_log, "[PLANI LP] El ultimo proceso ingresado a NEW no es menor que los demás, rechazando");
                    aumentar_procesos_rechazados();
                    UNLOCK_CON_LOG(mutex_cola_new);
                    UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                    continue;
                }
            }
        }
        else
        {
            LOG_ERROR(kernel_log, "[PLANI LP] Cola NEW vacia o algoritmo de ingreso a READY no reconocido");
            UNLOCK_CON_LOG(mutex_cola_new);
            UNLOCK_CON_LOG(mutex_inicializacion_procesos);
            terminar_kernel(EXIT_FAILURE);
        }

        if (!pcb)
        {
            LOG_ERROR(kernel_log, "[PLANI LP] No se pudo obtener un PCB de la cola NEW");
            UNLOCK_CON_LOG(mutex_cola_new);
            UNLOCK_CON_LOG(mutex_inicializacion_procesos);
            terminar_kernel(EXIT_FAILURE);
        }

        LOG_DEBUG(kernel_log, "[PLANI LP] PCB con PID %d obtenido de la cola NEW", pcb->PID);

        UNLOCK_CON_LOG(mutex_cola_new);

        if (!inicializar_proceso_en_memoria(pcb))
        {
            aumentar_procesos_rechazados();
            UNLOCK_CON_LOG(mutex_inicializacion_procesos);
            continue;
        }

        cambiar_estado_pcb_mutex(pcb, READY);
        UNLOCK_CON_LOG(mutex_inicializacion_procesos);
    }

    terminar_hilo();
    return NULL;
}

void *gestionar_exit(void *arg)
{
    registrar_hilo_activo();
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    while (kernel_debe_continuar())
    {
        SEM_WAIT(sem_proceso_a_exit);

        LOCK_CON_LOG(mutex_cola_exit);

        if (list_is_empty(cola_exit))
        {
            UNLOCK_CON_LOG(mutex_cola_exit);
            if (kernel_debe_continuar()) {
                LOG_ERROR(kernel_log, "[PLANI LP] [EXIT] Se despertó pero no hay procesos en EXIT");
                //terminar_kernel(EXIT_FAILURE);
            } else {
                LOG_DEBUG(kernel_log, "[PLANI LP] [EXIT] Terminando: kernel cerrándose y cola EXIT vacía");
            }
            continue;
        }

        t_pcb *pcb = elegir_por_fifo(cola_exit);

        if (!pcb)
        {
            LOG_ERROR(kernel_log, "[PLANI LP] [EXIT] No se pudo obtener PCB desde EXIT");
            UNLOCK_CON_LOG(mutex_cola_exit);
            terminar_kernel(EXIT_FAILURE);
        }

        UNLOCK_CON_LOG(mutex_cola_exit);

        EXIT(&pcb);

        SEM_POST(sem_procesos_rechazados);
    }

    terminar_hilo();
    return NULL;
}

void *verificar_procesos_rechazados()
{
    registrar_hilo_activo();
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    while (kernel_debe_continuar())
    {
        SEM_WAIT(sem_procesos_rechazados);
        LOCK_CON_LOG(mutex_cola_susp_ready);
        LOCK_CON_LOG(mutex_inicializacion_procesos);

        if(!kernel_debe_continuar())
        {
            UNLOCK_CON_LOG(mutex_cola_susp_ready);
            UNLOCK_CON_LOG(mutex_inicializacion_procesos);
            break;
        }
        
        bool resultado = true;

        while (!list_is_empty(cola_susp_ready))
        {
            t_pcb *pcb_susp = NULL;
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
                LOG_ERROR(kernel_log, "[PLANI LP] [RECHAZADOS] Algoritmo de ingreso a READY no reconocido o cola SUSPENDED READY vacia");
                UNLOCK_CON_LOG(mutex_cola_susp_ready);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                terminar_kernel(EXIT_FAILURE);
            }

            if (!pcb_susp)
            {
                LOG_ERROR(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo obtener PCB desde SUSP READY");
                UNLOCK_CON_LOG(mutex_cola_susp_ready);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                terminar_kernel(EXIT_FAILURE);
            }

            // VERIFICAR ESTADO DEL KERNEL ANTES DE DESUSPENDER
            if (!kernel_debe_continuar()) {
                LOG_DEBUG(kernel_log, "[PLANI LP] [RECHAZADOS] Kernel terminando, cancelando desuspensión de PID %d", pcb_susp->PID);
                UNLOCK_CON_LOG(mutex_cola_susp_ready);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                break;
            }

            resultado = desuspender_proceso(pcb_susp);

            if (!resultado)
            {
                // Durante la terminación, ignorar errores de desuspensión
                if (!kernel_debe_continuar()) {
                    LOG_DEBUG(kernel_log, "[PLANI LP] [RECHAZADOS] Error de desuspensión ignorado durante terminación para PID %d", pcb_susp->PID);
                    UNLOCK_CON_LOG(mutex_cola_susp_ready);
                    UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                    break;
                }
                
                LOG_DEBUG(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo desuspender proceso en memoria para PID %d", pcb_susp->PID);
                UNLOCK_CON_LOG(mutex_cola_susp_ready);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                break;
            }
            else
            {
                LOG_DEBUG(kernel_log, "[PLANI LP] [RECHAZADOS] Proceso PID %d desuspendido correctamente", pcb_susp->PID);
                UNLOCK_CON_LOG(mutex_cola_susp_ready);
                cambiar_estado_pcb_mutex(pcb_susp, READY);
                LOCK_CON_LOG(mutex_cola_susp_ready);
                // disminuir_procesos_rechazados();
            }
        }

        UNLOCK_CON_LOG(mutex_cola_susp_ready);

        if (!resultado)
        {
            UNLOCK_CON_LOG(mutex_inicializacion_procesos);
            continue;
        }

        LOCK_CON_LOG(mutex_procesos_rechazados);

        while (procesos_new_rechazados > 0)
        {

            LOCK_CON_LOG(mutex_cola_new);

            t_pcb *pcb = NULL;
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
                LOG_ERROR(kernel_log, "[PLANI LP] [RECHAZADOS] Cola NEW vacia o algoritmo de ingreso a READY no reconocido");
                UNLOCK_CON_LOG(mutex_cola_new);
                UNLOCK_CON_LOG(mutex_procesos_rechazados);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                terminar_kernel(EXIT_FAILURE);
            }

            if (!pcb)
            {
                LOG_ERROR(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo obtener un PCB de la cola NEW");
                UNLOCK_CON_LOG(mutex_cola_new);
                UNLOCK_CON_LOG(mutex_procesos_rechazados);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                terminar_kernel(EXIT_FAILURE);
            }

            if (!inicializar_proceso_en_memoria(pcb))
            {
                LOG_DEBUG(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo inicializar proceso en memoria para PID %d", pcb->PID);
                UNLOCK_CON_LOG(mutex_cola_new);
                break;
            }
            else
            {
                LOG_DEBUG(kernel_log, "[PLANI LP] [RECHAZADOS] Proceso PID %d inicializado en memoria", pcb->PID);
                UNLOCK_CON_LOG(mutex_cola_new);
                cambiar_estado_pcb_mutex(pcb, READY);
                procesos_new_rechazados--;
            }
        }
        UNLOCK_CON_LOG(mutex_procesos_rechazados);
        UNLOCK_CON_LOG(mutex_inicializacion_procesos);
    }

    terminar_hilo();
    return NULL;
}

// AUXILIARES

void aumentar_procesos_rechazados()
{

    LOCK_CON_LOG(mutex_procesos_rechazados);

    procesos_new_rechazados++;
    LOG_DEBUG(kernel_log, "[PLANI LP] Aumentando procesos rechazados a %d", procesos_new_rechazados);
    UNLOCK_CON_LOG(mutex_procesos_rechazados);
}

bool hay_rechazados_esperando()
{

    LOCK_CON_LOG(mutex_procesos_rechazados);

    bool hay_rechazados = procesos_new_rechazados > 0;

    LOG_DEBUG(kernel_log, "[PLANI LP] hay_rechazados_esperando: hay %d rechazados esperando", procesos_new_rechazados);
    UNLOCK_CON_LOG(mutex_procesos_rechazados);
    return hay_rechazados;
}

void disminuir_procesos_rechazados()
{

    LOCK_CON_LOG(mutex_procesos_rechazados);

    if (procesos_new_rechazados > 0)
    {
        procesos_new_rechazados--;
        LOG_DEBUG(kernel_log, "[PLANI LP] disminuir_procesos_rechazados: Procesos rechazados disminuidos a %d", procesos_new_rechazados);
    }
    else
    {
        LOG_ERROR(kernel_log, "[PLANI LP] disminuir_procesos_rechazados: No hay procesos rechazados para disminuir");
        UNLOCK_CON_LOG(mutex_procesos_rechazados);
        terminar_kernel(EXIT_FAILURE);
    }
    UNLOCK_CON_LOG(mutex_procesos_rechazados);
}

t_pcb *elegir_por_pmcp(t_list *cola_a_utilizar)
{
    LOG_DEBUG(kernel_log, "[PLANI LP] ELIGIENDO POR PMCP (Proceso Mas Chico Primero)");

    if (list_is_empty(cola_a_utilizar))
    {
        LOG_ERROR(kernel_log, "[PLANI LP] PMCP: cola_a_utilizar vacía");
        return NULL;
    }

    LOG_DEBUG(kernel_log, "[PLANI LP] PMCP: seleccionando el PCB con menor tamaño de memoria");
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
    if (!candidato)
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