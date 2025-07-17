#include "../headers/planificadores.h"

void *timer_suspension(void *v_arg)
{
    t_timer_arg *arg = v_arg;
    t_pcb *pcb = arg->pcb;
    bool *flag = arg->vigente;
    int pid = arg->pid;

    if (!pcb || !arg || !flag)
    {
        log_debug(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión no inicializado por PCB o arg o flag null)"), pid);
        if (flag)
        {
            free(flag);
            flag = NULL;
        }
        free(arg);
        return NULL;
    }
    LOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);

    double inicio = fmax(0, (get_time() - pcb->tiempo_inicio_blocked));

    log_debug(kernel_log, AZUL("[PLANI MP] PID %d pasó a blocked hace %.3f ms"), pid, inicio);

    UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);

    usleep((TIEMPO_SUSPENSION - inicio) * 1000); // usleep usa microsegundos: 1 ms = 1000 µs

    if (!flag || !*flag)
    {
        log_debug(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por flag desactivada"), pid);
        if (flag)
        {
            free(flag);
            flag = NULL;
        }
        free(arg);
        return NULL;
    }
    else if (!pcb)
    {
        log_debug(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por PCB null)"), pid);
        if (flag)
        {
            free(flag);
            flag = NULL;
        }
        free(arg);
        return NULL;
    }

    LOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);
    if (!flag || !*flag || !pcb || pcb->Estado != BLOCKED || pcb->timer_flag != flag || pcb->tiempo_inicio_blocked < 0)
    {
        log_debug(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por haber salido de BLOCKED)"), pid);
        if (flag)
        {
            free(flag);
            flag = NULL;
        }
        free(arg);
        UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);
        return NULL;
    }

    log_debug(kernel_log, AZUL("[PLANI MP] Timer de suspensión expirado para PID=%d"), pcb->PID);

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
        log_error(kernel_log, "No se pudo suspender el proceso PID=%d", pcb->PID);
        UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);
        terminar_kernel(EXIT_FAILURE);
    }
    cambiar_estado_pcb(pcb, SUSP_BLOCKED);
    UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);

    log_debug(kernel_log, AZUL("[PLANI MP] Proceso PID %d suspendido correctamente"), pcb->PID);

    SEM_POST(sem_procesos_rechazados);

    return NULL;
}

void iniciar_timer_suspension(t_pcb *pcb)
{
    pthread_t hilo_timer;
    bool *flag = malloc(sizeof(bool));
    *flag = true;

    if (pcb->timer_flag)
    {
        log_error(kernel_log, "[PLANI MP] Ya existe un timer vigente para el PID %d, se invalidará", pcb->PID);
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
        log_error(kernel_log, "[PLANI MP] No se pudo crear hilo de suspensión para PID %d", pcb->PID);
        terminar_kernel(EXIT_FAILURE);
    }
    log_debug(kernel_log, AZUL("[PLANI MP] Hilo de suspensión creado para PID %d"), pcb->PID);
    pthread_detach(hilo_timer);
}