#include "../headers/syscalls.h"
#include "../headers/planificadores.h"
#include <time.h>

t_temporal* tiempo_estado_actual;

// Variable global para el siguiente PID
static int siguiente_pid = 1;

// Función para obtener el siguiente PID disponible
static int obtener_siguiente_pid() {
    return siguiente_pid++;
}

//////////////////////////////////////////////////////////// INIT PROC ////////////////////////////////////////////////////////////
void INIT_PROC(char* nombre_archivo, int tam_memoria) {
    log_trace(kernel_log, "INIT_PROC - Nombre archivo recibido: '%s'", nombre_archivo);
    
    // Crear nuevo PCB
    t_pcb* nuevo_proceso = malloc(sizeof(t_pcb));
    memset(nuevo_proceso, 0, sizeof(t_pcb));    // Inicializar todo en 0
    nuevo_proceso->PID = obtener_siguiente_pid();
    nuevo_proceso->Estado = NEW;  // Cambiar a NEW para transiciones válidas
    nuevo_proceso->tamanio_memoria = tam_memoria;
    nuevo_proceso->path = strdup(nombre_archivo);
    nuevo_proceso->PC = 1;  // Inicializar PC a 1
    nuevo_proceso->estimacion_rafaga = ESTIMACION_INICIAL;
    
    // Crear nueva conexión efímera a Memoria
    int fd_memoria_local = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, kernel_log);
    if (fd_memoria_local == -1) {
        log_error(kernel_log, "INIT_PROC: No se pudo crear conexión a Memoria para inicializar PID %d", nuevo_proceso->PID);
        free(nuevo_proceso->path);
        free(nuevo_proceso);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    int handshake = HANDSHAKE_MEMORIA_KERNEL;
    send(fd_memoria_local, &handshake, sizeof(int), 0);

    // Comunicarse con memoria para inicializar el proceso
    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
    agregar_a_paquete(paquete, &nuevo_proceso->PID, sizeof(int));
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);
    agregar_a_paquete(paquete, &tam_memoria, sizeof(int));
    enviar_paquete(paquete, fd_memoria_local);
    eliminar_paquete(paquete);
    
    // Esperar respuesta de memoria
    t_respuesta respuesta;
    if (recv(fd_memoria_local, &respuesta, sizeof(t_respuesta), 0) <= 0) {
        log_error(kernel_log, "Error al recibir respuesta de memoria para INIT_PROC");
        close(fd_memoria_local);
        free(nuevo_proceso->path);
        free(nuevo_proceso);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    close(fd_memoria_local);
    
    // Procesar respuesta
    if (respuesta == OK) {
        log_trace(kernel_log, "INIT_PROC: proceso inicializado exitosamente, pasa a READY");
        cambiar_estado_pcb(nuevo_proceso, READY);  
        log_info(kernel_log, VERDE("## (PID: %d) Se crea el proceso - Estado: "AZUL("READY")), nuevo_proceso->PID);
    } else {
        // Si no hay espacio en memoria, encolar en cola de rechazados para que el planificador de largo plazo lo maneje
        log_warning(kernel_log, "INIT_PROC: No hay espacio en memoria para PID %d, encolando en cola de rechazados", nuevo_proceso->PID);
        // El proceso ya está en NEW, solo encolarlo en rechazados
        encolar_proceso_rechazado(nuevo_proceso);
        log_info(kernel_log, VERDE("## (PID: %d) Se crea el proceso - Estado: "AZUL("RECHAZADO")" (esperando espacio en memoria)"), nuevo_proceso->PID);
    }
}

//////////////////////////////////////////////////////////// DUMP MEMORY ////////////////////////////////////////////////////////////
void DUMP_MEMORY(t_pcb* pcb_dump) {
    if (!pcb_dump) {
        log_error(kernel_log, "DUMP_MEMORY: PCB nulo");
        return;
    }

    // Cambiar estado del proceso a BLOCKED
    cambiar_estado_pcb(pcb_dump, BLOCKED);
    
    // Crear nueva conexión efímera a Memoria
    int fd_memoria_local = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, kernel_log);
    if (fd_memoria_local == -1) {
        log_error(kernel_log, "DUMP_MEMORY: No se pudo crear conexión a Memoria para PID %d", pcb_dump->PID);
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        return;
    }
    int handshake = HANDSHAKE_MEMORIA_KERNEL;
    send(fd_memoria_local, &handshake, sizeof(int), 0);

    // Enviar solicitud de DUMP_MEMORY a Memoria usando paquete
    t_paquete* paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_entero_a_paquete(paquete, pcb_dump->PID);
    enviar_paquete(paquete, fd_memoria_local);
    eliminar_paquete(paquete);
    
    log_trace(kernel_log, "DUMP_MEMORY_OP enviado a Memoria para PID=%d", pcb_dump->PID);
    
    // Esperar respuesta de memoria de forma síncrona
    t_respuesta respuesta;
    if (recv(fd_memoria_local, &respuesta, sizeof(t_respuesta), 0) <= 0) {
        log_error(kernel_log, "Error al recibir respuesta de memoria para DUMP_MEMORY PID %d", pcb_dump->PID);
        close(fd_memoria_local);
        // Si falla la recepción, mandar el proceso a EXIT
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        return;
    }
    close(fd_memoria_local);
    
    // Procesar la respuesta
    if (respuesta == OK) {
        // Si la operación fue exitosa, desbloquear el proceso (pasa a READY)
        cambiar_estado_pcb(pcb_dump, READY);
        log_trace(kernel_log, "## (PID: %d) finalizó DUMP_MEMORY exitosamente y pasa a READY", pcb_dump->PID);
        
        // ✅ Asegurar que el proceso se replanifique inmediatamente
        // Esto es importante para que el proceso continúe ejecutándose después del dump
        log_trace(kernel_log, "DUMP_MEMORY: Proceso %d listo para continuar ejecución", pcb_dump->PID);
    } else {
        // Si hubo error, enviar el proceso a EXIT
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        log_error(kernel_log, "## (PID: %d) - Error en DUMP_MEMORY, proceso enviado a EXIT", pcb_dump->PID);
    }
}

// Variables externas
extern t_list* lista_ios;
extern pthread_mutex_t mutex_ios;

//////////////////////////////////////////////////////////// IO ////////////////////////////////////////////////////////////

void IO(char* nombre_io, int tiempo_a_usar, t_pcb* pcb_a_io) {
    if (!pcb_a_io) {
        log_error(kernel_log, "IO: PCB nulo");
        return;
    }
      
    // Validar que la IO solicitada exista en el sistema
    io* dispositivo = get_io(nombre_io);
    
    if (dispositivo == NULL) {
        // Si no existe ninguna IO en el sistema con el nombre solicitado, el proceso se deberá enviar a EXIT
        log_debug(kernel_log, "IO: No existe el dispositivo '%s'", nombre_io);
        cambiar_estado_pcb(pcb_a_io, EXIT_ESTADO);
        return;
    }

    // En caso de que sí exista al menos una instancia de IO, aun si la misma se encuentre ocupada, el kernel deberá pasar el proceso al estado BLOCKED y agregarlo a la cola de bloqueados por la IO solicitada. 
    log_info(kernel_log, VERDE("## (PID: %d) - Bloqueado por IO: %s"), pcb_a_io->PID, nombre_io);
    log_debug(kernel_log, "## (PID: %d) - Bloqueado por IO: %s (tiempo: %d ms)", pcb_a_io->PID, nombre_io, tiempo_a_usar);  


    cambiar_estado_pcb(pcb_a_io, BLOCKED);
    // Aca se envía el proceso a la IO si existe
    bloquear_pcb_por_io(nombre_io, pcb_a_io, tiempo_a_usar);
}

//////////////////////////////////////////////////////////// EXIT ////////////////////////////////////////////////////////////
void EXIT(t_pcb* pcb_a_finalizar) {
    if (!pcb_a_finalizar) {
        log_error(kernel_log, "EXIT: PCB nulo");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Crear nueva conexión a Memoria para esta petición
    int fd_memoria_local = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, kernel_log);
    if (fd_memoria_local == -1) {
        log_error(kernel_log, "EXIT: No se pudo crear conexión a Memoria para finalizar PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    // Handshake, el protocolo lo requiere
    int handshake = HANDSHAKE_MEMORIA_KERNEL;
    send(fd_memoria_local, &handshake, sizeof(int), 0);

    // Notificar a Memoria
    log_debug(kernel_log, "EXIT: Preparando paquete FINALIZAR_PROC_OP para PID %d", pcb_a_finalizar->PID);
    t_paquete* paquete = crear_paquete_op(FINALIZAR_PROC_OP);
    agregar_entero_a_paquete(paquete, pcb_a_finalizar->PID);
    log_debug(kernel_log, "EXIT: Enviando paquete FINALIZAR_PROC_OP a Memoria para PID %d", pcb_a_finalizar->PID);
    enviar_paquete(paquete, fd_memoria_local);
    eliminar_paquete(paquete);
    log_debug(kernel_log, "EXIT: Paquete FINALIZAR_PROC_OP enviado a Memoria para PID %d", pcb_a_finalizar->PID);

    log_trace(kernel_log, "EXIT: Esperando confirmación de Memoria para PID %d...", pcb_a_finalizar->PID);
    t_respuesta confirmacion;
    int bytes_recv = recv(fd_memoria_local, &confirmacion, sizeof(t_respuesta), 0);
    log_debug(kernel_log, "EXIT: Recibidos %d bytes de confirmación de Memoria para PID %d", bytes_recv, pcb_a_finalizar->PID);
    close(fd_memoria_local);
    if (bytes_recv <= 0) {
        log_error(kernel_log, "EXIT: No se pudo recibir confirmación de Memoria para PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    
    if (confirmacion == OK) {
        log_trace(kernel_log, "EXIT: Memoria confirmó finalización de PID %d", pcb_a_finalizar->PID);
    } else if (confirmacion == ERROR) {
        log_error(kernel_log, "EXIT: Memoria rechazó la finalización de PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    } else {
        log_error(kernel_log, "EXIT: Respuesta desconocida de Memoria para PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Logs
    log_info(kernel_log, VERDE("## (PID: %d) - Finaliza el proceso"), pcb_a_finalizar->PID);
    loguear_metricas_estado(pcb_a_finalizar);

    // Eliminar de cola_exit, cola procesos, liberar pcb y cronometro
    log_debug(kernel_log, "EXIT: esperando mutex_cola_exit para eliminar de cola exit PCB PID=%d", pcb_a_finalizar->PID);
    pthread_mutex_lock(&mutex_cola_exit);
    log_debug(kernel_log, "EXIT: bloqueando mutex_cola_exit para eliminar de cola exit PCB PID=%d", pcb_a_finalizar->PID);
    list_remove_element(cola_exit, pcb_a_finalizar);
    pthread_mutex_unlock(&mutex_cola_exit);

    log_debug(kernel_log, "EXIT: esperando mutex_cola_exit para eliminar de cola procesos PCB PID=%d", pcb_a_finalizar->PID);
    pthread_mutex_lock(&mutex_cola_procesos);
    log_debug(kernel_log, "EXIT: bloqueando mutex_cola_exit para eliminar de cola procesos PCB PID=%d", pcb_a_finalizar->PID);
    list_remove_element(cola_procesos, pcb_a_finalizar);
    pthread_mutex_unlock(&mutex_cola_procesos);

    char* pid_key = string_itoa(pcb_a_finalizar->PID);
    dictionary_remove_and_destroy(tiempos_por_pid, pid_key, (void*) temporal_destroy);
    free(pid_key);

    free(pcb_a_finalizar->path);
    free(pcb_a_finalizar);

    // Loguear el estado de todas las colas tras finalizar el proceso
    mostrar_colas_estados();

    // Notificar a planificador LP
    sem_post(&sem_finalizacion_de_proceso);

    // Notificar al planificador de largo plazo que puede intentar avanzar procesos de NEW
    sem_post(&sem_proceso_a_new);

   // Hay que Comentar para que Kernel no finalice en las pruebas
    // Verificar si no quedan procesos en el sistema
    log_debug(kernel_log, "EXIT: verificando si quedan procesos en el sistema");
    
    pthread_mutex_lock(&mutex_cola_new);
    pthread_mutex_lock(&mutex_cola_ready);
    pthread_mutex_lock(&mutex_cola_running);
    pthread_mutex_lock(&mutex_cola_blocked);
    pthread_mutex_lock(&mutex_cola_susp_ready);
    pthread_mutex_lock(&mutex_cola_susp_blocked);
    pthread_mutex_lock(&mutex_cola_procesos);
    
    int total_procesos = list_size(cola_new) + list_size(cola_ready) + 
                        list_size(cola_running) + list_size(cola_blocked) + 
                        list_size(cola_susp_ready) + list_size(cola_susp_blocked) + 
                        list_size(cola_procesos);
    
    pthread_mutex_unlock(&mutex_cola_procesos);
    pthread_mutex_unlock(&mutex_cola_susp_blocked);
    pthread_mutex_unlock(&mutex_cola_susp_ready);
    pthread_mutex_unlock(&mutex_cola_blocked);
    pthread_mutex_unlock(&mutex_cola_running);
    pthread_mutex_unlock(&mutex_cola_ready);
    pthread_mutex_unlock(&mutex_cola_new);
    
    log_trace(kernel_log, "EXIT: Total de procesos restantes en el sistema: %d", total_procesos);
    
    if(total_procesos == 0) {
        mostrar_colas_estados();
        log_trace(kernel_log, "EXIT: No quedan procesos en el sistema, activando shutdown automático");
        activar_shutdown_automatico();
    }
}
