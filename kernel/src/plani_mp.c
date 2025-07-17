#include "../headers/planificadores.h"

void *timer_suspension(void *v_arg)
{
    t_timer_arg *arg = v_arg;
    t_pcb *pcb = arg->pcb;
    bool *flag = arg->vigente;
    int pid = arg->pid;
    double inicio = fmax(0, (get_time() - pcb->tiempo_inicio_blocked));

    log_trace(kernel_log, AZUL("[PLANI MP] PID %d pasó a blocked hace %.3f ms"), pid, inicio);

    usleep((TIEMPO_SUSPENSION - inicio) * 1000); // usleep usa microsegundos: 1 ms = 1000 µs

    if (!pcb || pcb == NULL)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por PCB null)"), pid);
        if (flag)
            free(flag);
        free(arg);
        return NULL;
    }

    pthread_mutex_lock(&pcb->mutex);
    if (!*flag)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por flag desactivada"), pid);
        if (flag)
            free(flag);
        free(arg);
        pthread_mutex_unlock(&pcb->mutex);
        return NULL;
    }
    else if (pcb->Estado != BLOCKED)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por estado no BLOCKED)"), pid);
        if (flag)
            free(flag);
        free(arg);
        pthread_mutex_unlock(&pcb->mutex);
        return NULL;
    }
    else if (pcb->tiempo_inicio_blocked < 0)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por tiempo_inicio_blocked no inicializado)"), pid);
        if (flag)
            free(flag);
        free(arg);
        pthread_mutex_unlock(&pcb->mutex);
        return NULL;
    }
    else if (pcb->timer_flag != flag)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por flag distinto"), pid);
        if (flag)
            free(flag);
        free(arg);
        pthread_mutex_unlock(&pcb->mutex);
        return NULL;
    }

    log_trace(kernel_log, AZUL("[PLANI MP] Timer de suspensión expirado para PID=%d"), pcb->PID);

    if (pcb->timer_flag == flag)
        pcb->timer_flag = NULL;

    cambiar_estado_pcb(pcb, SUSP_BLOCKED);

    if (flag)
        free(flag);
    free(arg);

    if (!suspender_proceso(pcb))
    {
        log_error(kernel_log, "No se pudo suspender el proceso PID=%d", pcb->PID);
        pthread_mutex_unlock(&pcb->mutex);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&pcb->mutex);

    log_trace(kernel_log, AZUL("[PLANI MP] Proceso PID %d suspendido correctamente"), pcb->PID);

    verificar_procesos_rechazados();

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

    pcb->timer_flag = flag; // mutex?

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
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    log_trace(kernel_log, AZUL("[PLANI MP] Hilo de suspensión creado para PID %d"), pcb->PID);
    pthread_detach(hilo_timer);
}