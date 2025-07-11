#include "../headers/planificadores.h"

void *timer_suspension(void *v_arg)
{
    t_timer_arg *arg = v_arg;
    t_pcb *pcb = arg->pcb;
    bool *flag = arg->vigente;
    int pid = arg->pid;

    usleep(TIEMPO_SUSPENSION * 1000);     // usleep usa microsegundos: 1 ms = 1000 µs

    if (!pcb)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por PCB null)"), pid);
        if (flag)
            free(flag);
        free(arg);
        return NULL;
    }
    else if (pcb->tiempo_inicio_blocked < 0)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por tiempo_inicio_blocked no inicializado)"), pid);
        if (flag)
            free(flag);
        free(arg);
        return NULL;
    }
    else if (pcb->Estado != BLOCKED)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por estado no BLOCKED)"), pid);
        if (flag)
            free(flag);
        free(arg);
        return NULL;
    }
    else if (pcb->timer_flag != flag)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por flag distinto"), pid);
        if (flag)
            free(flag);
        free(arg);
        return NULL;
    }
    else if (!*flag)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] (%d Timer de suspensión ignorado por flag no vigente"), pid);
        if (flag)
            free(flag);
        free(arg);
        return NULL;
    }

    log_trace(kernel_log, AZUL("[PLANI MP] Timer de suspensión expirado para PID=%d"), pcb->PID);

    t_paquete *paquete = crear_paquete_op(SUSPENDER_PROCESO_OP);
    agregar_entero_a_paquete(paquete, pcb->PID);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    t_respuesta respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0 || respuesta != OK)
    {
        log_error(kernel_log, "[PLANI MP] Error al suspender proceso PID %d", pcb->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, AZUL("[PLANI MP] Proceso PID %d suspendido correctamente"), pcb->PID);

    if (pcb->timer_flag == flag)
        pcb->timer_flag = NULL;

    cambiar_estado_pcb(pcb, SUSP_BLOCKED);

    free(flag);
    free(arg);

    // TODO REPLANIFICAR

    return NULL;
}

void iniciar_timer_suspension(t_pcb *pcb)
{
    pthread_t hilo_timer;
    bool *flag = malloc(sizeof(bool));
    *flag = true;

    if (pcb->timer_flag)
    {
        *(pcb->timer_flag) = false;
    }

    pcb->timer_flag = flag;     // mutex?

    t_timer_arg *arg = malloc(sizeof(t_timer_arg));
    arg->pcb = pcb;
    arg->vigente = flag;
    arg->pid = pcb->PID;

    if (pthread_create(&hilo_timer, NULL, timer_suspension, arg) == 0)
    {
        log_trace(kernel_log, AZUL("[PLANI MP] Hilo de suspensión creado para PID %d"), pcb->PID);
        pthread_detach(hilo_timer);
    }
    else
    {
        pcb->timer_flag = NULL;
        free(flag);
        free(arg);
        log_error(kernel_log, "[PLANI MP] No se pudo crear hilo de suspensión para PID %d", pcb->PID);
    }
}