#include "../headers/syscalls.h"
#include "../headers/planificadores.h"
#include <time.h>

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
    pthread_mutex_init(&nuevo_proceso->mutex, NULL);

    cambiar_estado_pcb_mutex(nuevo_proceso, NEW);
    log_info(kernel_log, CYAN("## (%d) Se crea el proceso - Estado: ") AZUL("NEW"), nuevo_proceso->PID);
}

//////////////////////////////////////////////////////////// IO ////////////////////////////////////////////////////////////

void IO(char *nombre_io, int tiempo_a_usar, t_pcb *pcb_a_io)
{
    if (!pcb_a_io)
    {
        LOG_TRACE(kernel_log, "PCB nulo");
        return;
    }

    io *dispositivo = get_io(nombre_io);

    if (!dispositivo)
    {
        LOG_TRACE(kernel_log, "No existe el dispositivo '%s'", nombre_io);
        cambiar_estado_pcb_mutex(pcb_a_io, EXIT_ESTADO);
        return;
    }

    log_info(kernel_log, PURPURA("## (%d) - Bloqueado por IO: %s"), pcb_a_io->PID, nombre_io);
    LOG_TRACE(kernel_log, "## (%d) - Bloqueado por IO: %s (tiempo: %d ms)", pcb_a_io->PID, nombre_io, tiempo_a_usar);

    cambiar_estado_pcb_mutex(pcb_a_io, BLOCKED);
    bloquear_pcb_por_io(nombre_io, pcb_a_io, tiempo_a_usar);
}

//////////////////////////////////////////////////////////// EXIT ////////////////////////////////////////////////////////////

void EXIT(t_pcb **ptr_pcb_a_finalizar)
{
    if (!ptr_pcb_a_finalizar || !(*ptr_pcb_a_finalizar))
    {
        LOG_TRACE(kernel_log, "PCB nulo o puntero a PCB nulo");
        return;
    }
    t_pcb *pcb_a_finalizar = *ptr_pcb_a_finalizar;

    LOCK_CON_LOG(mutex_cola_exit);
    LOCK_CON_LOG_PCB(pcb_a_finalizar->mutex, pcb_a_finalizar->PID);
    if (!finalizar_proceso_en_memoria(pcb_a_finalizar->PID))
    {
        LOG_TRACE(kernel_log, "Memoria rechazó FINALIZAR_PROC_OP para PID %d", pcb_a_finalizar->PID);
        UNLOCK_CON_LOG_PCB(pcb_a_finalizar->mutex, pcb_a_finalizar->PID);
        UNLOCK_CON_LOG(mutex_cola_exit);
        return;
    }

    log_info(kernel_log, ROJO("## (%d) - Finaliza el proceso"), pcb_a_finalizar->PID);
    actualizar_metricas_finalizacion(pcb_a_finalizar);
    loguear_metricas_estado(pcb_a_finalizar);

    liberar_pcb(pcb_a_finalizar);
    *ptr_pcb_a_finalizar = NULL;

    UNLOCK_CON_LOG(mutex_cola_exit);

    verificar_procesos_restantes();
}

void actualizar_metricas_finalizacion(t_pcb *pcb)
{
    if (!pcb)
    {
        LOG_TRACE(kernel_log, "PCB nulo");
        return;
    }

    char *pid_key = string_itoa(pcb->PID);
    t_temporal *cronometro = dictionary_get(tiempos_por_pid, pid_key);

    if (cronometro)
    {
        temporal_stop(cronometro);
        int64_t tiempo = temporal_gettime(cronometro); // Tiempo en milisegundos

        // Actualizar métricas en EXIT
        pcb->MT[EXIT_ESTADO] += (int)tiempo;
        pcb->ME[EXIT_ESTADO] = 1;

        LOG_TRACE(kernel_log,
                  "## (%d) - Métricas actualizadas: ME[EXIT]=%d, MT[EXIT]=%d ms",
                  pcb->PID,
                  pcb->ME[EXIT_ESTADO],
                  pcb->MT[EXIT_ESTADO]);

        temporal_destroy(cronometro);
        dictionary_remove(tiempos_por_pid, pid_key);
    }
    else
    {
        LOG_TRACE(kernel_log, "No se encontró cronómetro activo para PID %d al finalizar", pcb->PID);
    }

    free(pid_key);
}

//////////////////////////////////////////////////////////// DUMP MEMORY ////////////////////////////////////////////////////////////

void DUMP_MEMORY(t_pcb *pcb_dump)
{
    if (!pcb_dump)
    {
        LOG_TRACE(kernel_log, "PCB nulo");
        return;
    }

    if (dump_memory(pcb_dump->PID))
    {
        cambiar_estado_pcb_mutex(pcb_dump, READY);
        LOG_TRACE(kernel_log, "## (%d) finalizó DUMP_MEMORY exitosamente y pasa a READY", pcb_dump->PID);
    }
    else
    {
        cambiar_estado_pcb_mutex(pcb_dump, EXIT_ESTADO);
        LOG_TRACE(kernel_log, "## (%d) - Error en DUMP_MEMORY, proceso enviado a EXIT", pcb_dump->PID);
    }
}