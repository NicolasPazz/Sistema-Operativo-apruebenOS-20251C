#include "../headers/planificadores.h"
#include "../headers/MEMKernel.h"

int procesos_new_rechazados = 0;

/**
 * @brief Planificador de largo plazo del kernel.
 *
 * Este hilo se encarga de gestionar el ingreso de procesos desde la cola NEW a la cola READY,
 * siguiendo el algoritmo de ingreso configurado (FIFO o PMCP).
 * - Espera a que haya procesos en la cola NEW y verifica que la cola SUSPENDED READY esté vacía.
 * - Si hay procesos rechazados esperando, rechaza nuevos procesos según la política.
 * - Selecciona el proceso a ingresar a READY según el algoritmo configurado:
 *   - FIFO: selecciona el primero en la cola.
 *   - PMCP: selecciona el último si es el de menor tamaño, de lo contrario lo rechaza.
 * - Inicializa el proceso en memoria y lo mueve a READY si es posible.
 * - Utiliza semáforos y mutex para la sincronización entre hilos.
 * - Registra eventos y estados en el log del kernel para facilitar el debugging.
 *
 * @param arg Argumento para el hilo (no utilizado).
 * @return NULL al finalizar el hilo o en caso de error.
 */
void *planificador_largo_plazo(void *arg)
{
    LOCK_CON_LOG(mutex_planificador_lp);
    LOG_TRACE(kernel_log, "=== PLANIFICADOR LP INICIADO ===");
    while (1)
    {
        SEM_WAIT(sem_proceso_a_new);

        LOCK_CON_LOG(mutex_cola_susp_ready);
        bool susp_vacia = list_is_empty(cola_susp_ready);
        UNLOCK_CON_LOG(mutex_cola_susp_ready);

        if (!susp_vacia)
        {
            LOG_TRACE(kernel_log, "[PLANI LP] Cola SUSPENDED READY no está vacía, se debera esperar a que se libere un proceso");
            aumentar_procesos_rechazados();
            continue;
        }

        LOCK_CON_LOG(mutex_inicializacion_procesos);
        LOCK_CON_LOG(mutex_cola_new);

        t_pcb *pcb = NULL;
        int cant_new = list_size(cola_new);

        if (cant_new == 0)
        {
            LOG_TRACE(kernel_log, "[PLANI LP] Cola NEW vacia");
            UNLOCK_CON_LOG(mutex_cola_new);
            UNLOCK_CON_LOG(mutex_inicializacion_procesos);
            return NULL;
        }

        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0)
        {
            if (hay_rechazados_esperando())
            {
                LOG_TRACE(kernel_log, "[PLANI LP] Cola NEW con procesos rechazados, rechazando nuevos procesos");
                aumentar_procesos_rechazados();
                UNLOCK_CON_LOG(mutex_cola_new);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                continue;
            }
            else if (cant_new >= 1)
            {
                LOG_TRACE(kernel_log, "[PLANI LP] Cola NEW sin procesos rechazados, eligiendo por FIFO");
                pcb = elegir_por_fifo(cola_new);
            }
        }
        else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0)
        {
            if (cant_new == 1)
            {
                LOG_TRACE(kernel_log, "[PLANI LP] Cola NEW con un solo proceso, eligiendo por FIFO");
                pcb = elegir_por_fifo(cola_new);
            }
            else if (cant_new > 1)
            {
                LOG_TRACE(kernel_log, "[PLANI LP] Cola NEW con varios procesos, eligiendo por PMCP");
                pcb = ultimo_si_es_menor();
                if (pcb)
                {
                    LOG_TRACE(kernel_log, "[PLANI LP] Último proceso ingresado a NEW con PID %d es el de menor tamaño", pcb->PID);
                }
                else
                {
                    LOG_TRACE(kernel_log, "[PLANI LP] El ultimo proceso ingresado a NEW no es menor que los demás, rechazando");
                    aumentar_procesos_rechazados();
                    UNLOCK_CON_LOG(mutex_cola_new);
                    UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                    continue;
                }
            }
        }
        else
        {
            LOG_TRACE(kernel_log, "[PLANI LP] Cola NEW vacia o algoritmo de ingreso a READY no reconocido");
            UNLOCK_CON_LOG(mutex_cola_new);
            UNLOCK_CON_LOG(mutex_inicializacion_procesos);
            return NULL;
        }

        if (!pcb)
        {
            LOG_TRACE(kernel_log, "[PLANI LP] No se pudo obtener un PCB de la cola NEW");
            UNLOCK_CON_LOG(mutex_cola_new);
            UNLOCK_CON_LOG(mutex_inicializacion_procesos);
            return NULL;
        }

        LOG_TRACE(kernel_log, "[PLANI LP] PCB con PID %d obtenido de la cola NEW", pcb->PID);

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

    return NULL;
}

/**
 * @brief Gestiona la finalización de procesos en la cola EXIT.
 *
 * Este hilo espera a que haya procesos en la cola EXIT, adquiere el mutex correspondiente,
 * verifica si la cola está vacía y, en ese caso, libera el mutex y termina. Si hay procesos,
 * selecciona uno por FIFO, lo procesa llamando a EXIT y luego señala que hay procesos rechazados.
 * El hilo está preparado para ser cancelado de forma diferida.
 *
 * @param arg Argumento para el hilo (no utilizado).
 * @return NULL al finalizar la gestión o si no hay procesos en EXIT.
 */
void *gestionar_exit(void *arg)
{
    while (1)
    {
        SEM_WAIT(sem_proceso_a_exit);

        LOCK_CON_LOG(mutex_cola_exit);

        if (list_is_empty(cola_exit))
        {
            UNLOCK_CON_LOG(mutex_cola_exit);
            LOG_TRACE(kernel_log, "[PLANI LP] [EXIT] Se despertó pero no hay procesos en EXIT");
            return NULL;
        }

        t_pcb *pcb = elegir_por_fifo(cola_exit);

        if (!pcb)
        {
            LOG_TRACE(kernel_log, "[PLANI LP] [EXIT] No se pudo obtener PCB desde EXIT");
            UNLOCK_CON_LOG(mutex_cola_exit);
            return NULL;
        }

        UNLOCK_CON_LOG(mutex_cola_exit);

        EXIT(&pcb);

        SEM_POST(sem_liberacion_memoria);
    }

    return NULL;
}

/**
 * @brief Verifica y gestiona los procesos rechazados en el sistema.
 *
 * Esta función se ejecuta en un hilo separado y se encarga de revisar periódicamente
 * los procesos que han sido rechazados, tanto en la cola de procesos suspendidos (SUSP READY)
 * como en la cola de procesos nuevos (NEW). Según el algoritmo de ingreso a READY configurado
 * (FIFO o PMCP), selecciona el proceso correspondiente para intentar desuspenderlo o inicializarlo
 * en memoria. Si la operación es exitosa, el proceso cambia su estado a READY.
 *
 * La función utiliza semáforos y mutex para garantizar la sincronización entre hilos y evitar
 * condiciones de carrera. Además, realiza logging detallado para facilitar el seguimiento
 * de las operaciones y posibles errores.
 *
 * @return NULL al finalizar la ejecución del hilo.
 */
void *verificar_procesos_rechazados()
{
    while (1)
    {
        SEM_WAIT(sem_liberacion_memoria);
        LOCK_CON_LOG(mutex_cola_susp_ready);
        LOCK_CON_LOG(mutex_inicializacion_procesos);

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
                LOG_TRACE(kernel_log, "[PLANI LP] [RECHAZADOS] Algoritmo de ingreso a READY no reconocido o cola SUSPENDED READY vacia");
                UNLOCK_CON_LOG(mutex_cola_susp_ready);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                return NULL;
            }

            if (!pcb_susp)
            {
                LOG_TRACE(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo obtener PCB desde SUSP READY");
                UNLOCK_CON_LOG(mutex_cola_susp_ready);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                return NULL;
            }

            resultado = desuspender_proceso(pcb_susp);

            if (!resultado)
            {
                LOG_TRACE(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo desuspender proceso en memoria para PID %d", pcb_susp->PID);
                UNLOCK_CON_LOG(mutex_cola_susp_ready);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                break;
            }
            else
            {
                LOG_TRACE(kernel_log, "[PLANI LP] [RECHAZADOS] Proceso PID %d desuspendido correctamente", pcb_susp->PID);
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
                LOG_TRACE(kernel_log, "[PLANI LP] [RECHAZADOS] Cola NEW vacia o algoritmo de ingreso a READY no reconocido");
                UNLOCK_CON_LOG(mutex_cola_new);
                UNLOCK_CON_LOG(mutex_procesos_rechazados);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                return NULL;
            }

            if (!pcb)
            {
                LOG_TRACE(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo obtener un PCB de la cola NEW");
                UNLOCK_CON_LOG(mutex_cola_new);
                UNLOCK_CON_LOG(mutex_procesos_rechazados);
                UNLOCK_CON_LOG(mutex_inicializacion_procesos);
                return NULL;
            }

            if (!inicializar_proceso_en_memoria(pcb))
            {
                LOG_TRACE(kernel_log, "[PLANI LP] [RECHAZADOS] No se pudo inicializar proceso en memoria para PID %d", pcb->PID);
                UNLOCK_CON_LOG(mutex_cola_new);
                break;
            }
            else
            {
                LOG_TRACE(kernel_log, "[PLANI LP] [RECHAZADOS] Proceso PID %d inicializado en memoria", pcb->PID);
                UNLOCK_CON_LOG(mutex_cola_new);
                cambiar_estado_pcb_mutex(pcb, READY);
                procesos_new_rechazados--;
            }
        }
        UNLOCK_CON_LOG(mutex_procesos_rechazados);
        UNLOCK_CON_LOG(mutex_inicializacion_procesos);
    }

    return NULL;
}

// AUXILIARES

void aumentar_procesos_rechazados()
{

    LOCK_CON_LOG(mutex_procesos_rechazados);

    procesos_new_rechazados++;
    LOG_TRACE(kernel_log, "[PLANI LP] Aumentando procesos rechazados a %d", procesos_new_rechazados);
    UNLOCK_CON_LOG(mutex_procesos_rechazados);
}

bool hay_rechazados_esperando()
{

    LOCK_CON_LOG(mutex_procesos_rechazados);

    bool hay_rechazados = procesos_new_rechazados > 0;

    LOG_TRACE(kernel_log, "[PLANI LP] hay_rechazados_esperando: hay %d rechazados esperando", procesos_new_rechazados);
    UNLOCK_CON_LOG(mutex_procesos_rechazados);
    return hay_rechazados;
}

void disminuir_procesos_rechazados()
{

    LOCK_CON_LOG(mutex_procesos_rechazados);

    if (procesos_new_rechazados > 0)
    {
        procesos_new_rechazados--;
        LOG_TRACE(kernel_log, "[PLANI LP] disminuir_procesos_rechazados: Procesos rechazados disminuidos a %d", procesos_new_rechazados);
    }
    else
    {
        LOG_TRACE(kernel_log, "[PLANI LP] disminuir_procesos_rechazados: No hay procesos rechazados para disminuir");
        UNLOCK_CON_LOG(mutex_procesos_rechazados);
        return;
    }
    UNLOCK_CON_LOG(mutex_procesos_rechazados);
}

t_pcb *elegir_por_pmcp(t_list *cola_a_utilizar)
{
    LOG_TRACE(kernel_log, "[PLANI LP] ELIGIENDO POR PMCP (Proceso Mas Chico Primero)");

    if (list_is_empty(cola_a_utilizar))
    {
        LOG_TRACE(kernel_log, "[PLANI LP] PMCP: cola_a_utilizar vacía");
        return NULL;
    }

    LOG_TRACE(kernel_log, "[PLANI LP] PMCP: seleccionando el PCB con menor tamaño de memoria");
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