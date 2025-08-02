#include "../headers/planificadores.h"

/**
 * @brief Hilo temporizador para la suspensión de un proceso.
 *
 * Esta función se ejecuta en un hilo separado y espera un tiempo determinado (TIEMPO_SUSPENSION)
 * desde que el proceso pasó a estado BLOCKED. Si el proceso sigue en estado BLOCKED y la bandera
 * de suspensión sigue activa, se procede a suspender el proceso y cambiar su estado a SUSP_BLOCKED.
 *
 * @param v_arg Puntero a una estructura t_timer_arg que contiene el PCB, la bandera de vigencia y el PID.
 * @return Siempre retorna NULL.
 *
 * Comportamiento:
 * - Verifica que los argumentos sean válidos.
 * - Calcula el tiempo transcurrido desde que el proceso pasó a BLOCKED.
 * - Espera el tiempo restante para la suspensión.
 * - Si la bandera de suspensión sigue activa y el proceso sigue en BLOCKED, lo suspende.
 * - Libera la memoria utilizada por la bandera y los argumentos.
 * - Realiza logging en cada paso relevante.
 * - Señaliza la liberación de memoria mediante un semáforo.
 */
void *timer_suspension(void *v_arg)
{
    t_timer_arg *arg = v_arg;
    t_pcb *pcb = arg->pcb;
    bool *flag = arg->vigente;
    int pid = arg->pid;

    if (!pcb || !arg || !flag)
    {
        LOG_TRACE(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión no inicializado por PCB o arg o flag null)"), pid);
        if (flag)
        {
            free(flag);
        }
        free(arg);

        return NULL;
    }
    LOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);

    double inicio = fmax(0, (get_time() - pcb->tiempo_inicio_blocked));

    LOG_TRACE(kernel_log, AZUL("[PLANI MP] PID %d pasó a blocked hace %.3f ms"), pid, inicio);

    UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);

    usleep((TIEMPO_SUSPENSION - inicio) * 1000); // usleep usa microsegundos: 1 ms = 1000 µs

    if (!flag || !*flag)
    {
        LOG_TRACE(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por flag desactivada"), pid);
        if (flag)
        {
            free(flag);
        }
        free(arg);

        return NULL;
    }
    else if (!pcb)
    {
        LOG_TRACE(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por PCB null)"), pid);
        if (flag)
        {
            free(flag);
        }
        free(arg);

        return NULL;
    }

    LOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);
    if (!flag || !*flag || !pcb || pcb->Estado != BLOCKED || pcb->timer_flag != flag || pcb->tiempo_inicio_blocked < 0)
    {
        LOG_TRACE(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por haber salido de BLOCKED)"), pid);
        if (flag)
        {
            free(flag);
        }
        free(arg);
        UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);

        return NULL;
    }

    LOG_TRACE(kernel_log, AZUL("[PLANI MP] Timer de suspensión expirado para PID=%d"), pcb->PID);

    if (pcb->timer_flag == flag)
        pcb->timer_flag = NULL;

    if (flag)
    {
        free(flag);
        flag = NULL;
    }
    free(arg);

    if (!suspender_proceso(pcb))
    {
        LOG_DEBUG(kernel_log, "No se pudo suspender el proceso PID=%d", pcb->PID);
        UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);
        return NULL;
    }
    cambiar_estado_pcb(pcb, SUSP_BLOCKED);
    UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);

    LOG_DEBUG(kernel_log, AZUL("[PLANI MP] Proceso PID %d suspendido correctamente"), pcb->PID);

    SEM_POST(sem_liberacion_memoria);

    return NULL;
}

void iniciar_timer_suspension(t_pcb *pcb)
{
    pthread_t hilo_timer; // Usamos variable local en lugar de malloc
    bool *flag = malloc(sizeof(bool));
    *flag = true;

    if (pcb->timer_flag)
    {
        LOG_TRACE(kernel_log, "[PLANI MP] Ya existe un timer vigente para el PID %d, se invalidará", pcb->PID);
        *(pcb->timer_flag) = false;
    }

    pcb->timer_flag = flag;

    t_timer_arg *arg = malloc(sizeof(t_timer_arg));
    arg->pcb = pcb;
    arg->vigente = flag;
    arg->pid = pcb->PID;

    if (pthread_create(&hilo_timer, NULL, timer_suspension, arg) != 0)
    {
        pcb->timer_flag = NULL;
        free(flag);
        free(arg);
        LOG_ERROR(kernel_log, "[PLANI MP] No se pudo crear hilo de suspensión para PID %d", pcb->PID);
        return;
    }
    pthread_detach(hilo_timer);
}