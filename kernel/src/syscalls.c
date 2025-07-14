#include "../headers/syscalls.h"
#include "../headers/planificadores.h"
#include <time.h>

// t_temporal *tiempo_estado_actual;

// Variable global para el siguiente PID
static int siguiente_pid = 0;
// Variables externas
extern t_list *lista_ios;
extern pthread_mutex_t mutex_ios;

// Función para obtener el siguiente PID disponible
static int obtener_siguiente_pid()
{
    return siguiente_pid++;
}

//////////////////////////////////////////////////////////// INIT PROC ////////////////////////////////////////////////////////////

void INIT_PROC(char *nombre_archivo, int tam_memoria)
{
    t_pcb *nuevo_proceso = malloc(sizeof(t_pcb));
    memset(nuevo_proceso, 0, sizeof(t_pcb)); // Inicializar todo en 0
    nuevo_proceso->PID = obtener_siguiente_pid();
    nuevo_proceso->Estado = INIT;
    nuevo_proceso->tamanio_memoria = tam_memoria;
    nuevo_proceso->path = strdup(nombre_archivo);
    nuevo_proceso->estimacion_rafaga = ESTIMACION_INICIAL;
    nuevo_proceso->tiempo_inicio_exec = -1;
    nuevo_proceso->tiempo_inicio_blocked = -1;

    cambiar_estado_pcb(nuevo_proceso, NEW);
    log_info(kernel_log, CYAN("## (%d) Se crea el proceso - Estado: ") AZUL("NEW"), nuevo_proceso->PID);
}


//////////////////////////////////////////////////////////// IO ////////////////////////////////////////////////////////////

void IO(char *nombre_io, int tiempo_a_usar, t_pcb *pcb_a_io)
{
    if (!pcb_a_io)
    {
        log_error(kernel_log, "IO: PCB nulo");
        return;
    }

    io *dispositivo = get_io(nombre_io);

    if (!dispositivo)
    {
        log_trace(kernel_log, "IO: No existe el dispositivo '%s'", nombre_io);
        cambiar_estado_pcb(pcb_a_io, EXIT_ESTADO);
        return;
    }

    log_info(kernel_log, PURPURA("## (%d) - Bloqueado por IO: %s"), pcb_a_io->PID, nombre_io);
    log_trace(kernel_log, "## (%d) - Bloqueado por IO: %s (tiempo: %d ms)", pcb_a_io->PID, nombre_io, tiempo_a_usar);

    cambiar_estado_pcb(pcb_a_io, BLOCKED);
    bloquear_pcb_por_io(nombre_io, pcb_a_io, tiempo_a_usar);
}

//////////////////////////////////////////////////////////// EXIT ////////////////////////////////////////////////////////////

void EXIT(t_pcb *pcb_a_finalizar)
{
    if (!pcb_a_finalizar)
    {
        log_error(kernel_log, "EXIT: PCB nulo");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "EXIT: esperando mutex_cola_exit para eliminar de cola exit PCB PID=%d", pcb_a_finalizar->PID);
    pthread_mutex_lock(&mutex_cola_exit);
    log_trace(kernel_log, "EXIT: bloqueando mutex_cola_exit para eliminar de cola exit PCB PID=%d", pcb_a_finalizar->PID);

    if (!finalizar_proceso_en_memoria(pcb_a_finalizar->PID))
    {
        log_error(kernel_log, "EXIT: Memoria rechazó FINALIZAR_PROC_OP para PID %d", pcb_a_finalizar->PID);
        pthread_mutex_unlock(&mutex_cola_exit);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_info(kernel_log, ROJO("## (%d) - Finaliza el proceso"), pcb_a_finalizar->PID);
    loguear_metricas_estado(pcb_a_finalizar);

    liberar_pcb(pcb_a_finalizar);

    pthread_mutex_unlock(&mutex_cola_exit);

    verificar_procesos_restantes();
}

//////////////////////////////////////////////////////////// DUMP MEMORY ////////////////////////////////////////////////////////////

void DUMP_MEMORY(t_pcb *pcb_dump)
{
    if (!pcb_dump)
    {
        log_error(kernel_log, "DUMP_MEMORY: PCB nulo");
        return;
    }

    if (dump_memory(pcb_dump->PID))
    {
        cambiar_estado_pcb(pcb_dump, READY);
        log_trace(kernel_log, "## (%d) finalizó DUMP_MEMORY exitosamente y pasa a READY", pcb_dump->PID);
    }
    else
    {
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        log_error(kernel_log, "## (%d) - Error en DUMP_MEMORY, proceso enviado a EXIT", pcb_dump->PID);
    }
}