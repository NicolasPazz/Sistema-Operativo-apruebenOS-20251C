#include "../headers/syscalls.h"
#include "../headers/planificadores.h"
#include <time.h>

// t_temporal *tiempo_estado_actual;

// Variable global para el siguiente PID
static int siguiente_pid = 0;

// Función para obtener el siguiente PID disponible
static int obtener_siguiente_pid()
{
    return siguiente_pid++;
}

//////////////////////////////////////////////////////////// INIT PROC ////////////////////////////////////////////////////////////

void INIT_PROC(char *nombre_archivo, int tam_memoria)
{
    log_trace(kernel_log, "INIT_PROC - Nombre archivo recibido: '%s'", nombre_archivo);

    // Crear nuevo PCB
    t_pcb *nuevo_proceso = malloc(sizeof(t_pcb));
    memset(nuevo_proceso, 0, sizeof(t_pcb)); // Inicializar todo en 0
    nuevo_proceso->PID = obtener_siguiente_pid();
    nuevo_proceso->Estado = INIT;
    nuevo_proceso->tamanio_memoria = tam_memoria;
    nuevo_proceso->path = strdup(nombre_archivo);
    nuevo_proceso->estimacion_rafaga = ESTIMACION_INICIAL;
    nuevo_proceso->tiempo_inicio_exec = -1;
    nuevo_proceso->tiempo_inicio_blocked = -1;

    log_trace(kernel_log, "INIT_PROC: proceso nuevo a la cola NEW");
    cambiar_estado_pcb(nuevo_proceso, NEW);
    log_info(kernel_log, CYAN("## (%d) Se crea el proceso - Estado: ") AZUL("NEW"), nuevo_proceso->PID);
}

//////////////////////////////////////////////////////////// DUMP MEMORY ////////////////////////////////////////////////////////////

void DUMP_MEMORY(t_pcb *pcb_dump)
{
    if (!pcb_dump)
    {
        log_error(kernel_log, "DUMP_MEMORY: PCB nulo");
        return;
    }

    // Cambiar estado del proceso a BLOCKED
    cambiar_estado_pcb(pcb_dump, BLOCKED);

    // Enviar solicitud de DUMP_MEMORY a Memoria usando paquete
    t_paquete *paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_entero_a_paquete(paquete, pcb_dump->PID);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    log_trace(kernel_log, "DUMP_MEMORY_OP enviado a Memoria para PID=%d", pcb_dump->PID);

    // Esperar respuesta de memoria de forma síncrona
    t_respuesta respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0)
    {
        log_error(kernel_log, "Error al recibir respuesta de memoria para DUMP_MEMORY PID %d", pcb_dump->PID);
        // Si falla la recepción, mandar el proceso a EXIT
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        return;
    }

    // Procesar la respuesta
    if (respuesta == OK)
    {
        // Si la operación fue exitosa, desbloquear el proceso (pasa a READY)
        cambiar_estado_pcb(pcb_dump, READY);
        log_trace(kernel_log, "## (%d) finalizó DUMP_MEMORY exitosamente y pasa a READY", pcb_dump->PID);

        // ✅ Asegurar que el proceso se replanifique inmediatamente
        // Esto es importante para que el proceso continúe ejecutándose después del dump
        log_trace(kernel_log, "DUMP_MEMORY: Proceso %d listo para continuar ejecución", pcb_dump->PID);
    }
    else
    {
        // Si hubo error, enviar el proceso a EXIT
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        log_error(kernel_log, "## (%d) - Error en DUMP_MEMORY, proceso enviado a EXIT", pcb_dump->PID);
    }
}

// Variables externas
extern t_list *lista_ios;
extern pthread_mutex_t mutex_ios;

//////////////////////////////////////////////////////////// IO ////////////////////////////////////////////////////////////

void IO(char *nombre_io, int tiempo_a_usar, t_pcb *pcb_a_io)
{
    if (!pcb_a_io)
    {
        log_error(kernel_log, "IO: PCB nulo");
        return;
    }

    // Validar que la IO solicitada exista en el sistema
    io *dispositivo = get_io(nombre_io);

    if (dispositivo == NULL)
    {
        // Si no existe ninguna IO en el sistema con el nombre solicitado, el proceso se deberá enviar a EXIT
        log_trace(kernel_log, "IO: No existe el dispositivo '%s'", nombre_io);
        cambiar_estado_pcb(pcb_a_io, EXIT_ESTADO);
        return;
    }

    // En caso de que sí exista al menos una instancia de IO, aun si la misma se encuentre ocupada, el kernel deberá pasar el proceso al estado BLOCKED y agregarlo a la cola de bloqueados por la IO solicitada.
    log_info(kernel_log, PURPURA("## (%d) - Bloqueado por IO: %s"), pcb_a_io->PID, nombre_io);
    log_trace(kernel_log, "## (%d) - Bloqueado por IO: %s (tiempo: %d ms)", pcb_a_io->PID, nombre_io, tiempo_a_usar);

    cambiar_estado_pcb(pcb_a_io, BLOCKED);
    // Aca se envía el proceso a la IO si existe
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

    log_debug(kernel_log, "EXIT: esperando mutex_cola_exit para eliminar de cola exit PCB PID=%d", pcb_a_finalizar->PID);
    pthread_mutex_lock(&mutex_cola_exit);
    log_debug(kernel_log, "EXIT: bloqueando mutex_cola_exit para eliminar de cola exit PCB PID=%d", pcb_a_finalizar->PID);

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

void verificar_procesos_restantes()
{
    log_trace(kernel_log, "EXIT: verificando si quedan procesos en el sistema");

    log_debug(kernel_log, "EXIT: esperando mutex_cola_new, mutex_cola_ready, mutex_cola_running, mutex_cola_blocked, mutex_cola_susp_ready, mutex_cola_susp_blocked, mutex_cola_exit y mutex_cola_procesos");
    pthread_mutex_lock(&mutex_cola_new);
    pthread_mutex_lock(&mutex_cola_ready);
    pthread_mutex_lock(&mutex_cola_running);
    pthread_mutex_lock(&mutex_cola_blocked);
    pthread_mutex_lock(&mutex_cola_susp_ready);
    pthread_mutex_lock(&mutex_cola_susp_blocked);
    pthread_mutex_lock(&mutex_cola_exit);
    pthread_mutex_lock(&mutex_cola_procesos);
    log_debug(kernel_log, "EXIT: bloqueando mutex_cola_new, mutex_cola_ready, mutex_cola_running, mutex_cola_blocked, mutex_cola_susp_ready, mutex_cola_susp_blocked, mutex_cola_exit y mutex_cola_procesos");

    int total_procesos = list_size(cola_new) + list_size(cola_ready) +
                         list_size(cola_running) + list_size(cola_blocked) +
                         list_size(cola_susp_ready) + list_size(cola_susp_blocked) +
                         list_size(cola_exit) + list_size(cola_procesos);

    pthread_mutex_unlock(&mutex_cola_procesos);
    pthread_mutex_unlock(&mutex_cola_exit);
    pthread_mutex_unlock(&mutex_cola_susp_blocked);
    pthread_mutex_unlock(&mutex_cola_susp_ready);
    pthread_mutex_unlock(&mutex_cola_blocked);
    pthread_mutex_unlock(&mutex_cola_running);
    pthread_mutex_unlock(&mutex_cola_ready);
    pthread_mutex_unlock(&mutex_cola_new);

    log_trace(kernel_log, "EXIT: Total de procesos restantes en el sistema: %d", total_procesos);

    if (total_procesos == 0)
    {
        mostrar_colas_estados();
        log_info(kernel_log, "Todos los procesos han terminado. Finalizando kernel...");
        terminar_kernel();
        exit(EXIT_SUCCESS);
    }
}