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
    nuevo_proceso->Estado = INIT;
    nuevo_proceso->tamanio_memoria = tam_memoria;
    nuevo_proceso->path = strdup(nombre_archivo);
    nuevo_proceso->PC = 0;  // Inicializar PC a 0
    nuevo_proceso->estimacion_rafaga = ESTIMACION_INICIAL;
    
    // Comunicarse con memoria para inicializar el proceso
    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
    agregar_a_paquete(paquete, &nuevo_proceso->PID, sizeof(int));
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);
    agregar_a_paquete(paquete, &tam_memoria, sizeof(int));
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
    
    // Esperar respuesta de memoria
    t_respuesta respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0) {
        log_error(kernel_log, "Error al recibir respuesta de memoria para INIT_PROC");
        free(nuevo_proceso->path);
        free(nuevo_proceso);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    
    // Procesar respuesta
    if (respuesta == OK) {
        log_trace(kernel_log, "INIT_PROC: proceso nuevo a la cola NEW");
        cambiar_estado_pcb(nuevo_proceso, NEW);  
        log_info(kernel_log, "\033[38;2;179;236;111m## (%d) Se crea el proceso - Estado: NEW\033[0m", nuevo_proceso->PID);
    } else {
        log_error(kernel_log, "Error al crear proceso en memoria");
        free(nuevo_proceso->path);
        free(nuevo_proceso);
        // Error al crear el proceso en memoria, no se puede continuar
        log_error(kernel_log, "No se pudo inicializar el proceso PID %d en memoria", nuevo_proceso->PID);
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
    
    // Enviar solicitud de DUMP_MEMORY a Memoria de forma síncrona
    op_code cod_op = DUMP_MEMORY_OP;
    if (send(fd_memoria, &cod_op, sizeof(op_code), 0) <= 0) {
        log_error(kernel_log, "Error al enviar DUMP_MEMORY_OP a Memoria para PID %d", pcb_dump->PID);
        // Si falla el envío, mandar el proceso a EXIT
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        return;
    }
    
    // Enviar PID del proceso
    if (send(fd_memoria, &pcb_dump->PID, sizeof(int), 0) <= 0) {
        log_error(kernel_log, "Error al enviar PID a Memoria para DUMP_MEMORY (PID: %d)", pcb_dump->PID);
        // Si falla el envío, mandar el proceso a EXIT
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        return;
    }
    
    log_trace(kernel_log, "DUMP_MEMORY_OP enviado a Memoria para PID=%d", pcb_dump->PID);
    
    // Esperar respuesta de memoria de forma síncrona
    t_respuesta respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0) {
        log_error(kernel_log, "Error al recibir respuesta de memoria para DUMP_MEMORY PID %d", pcb_dump->PID);
        // Si falla la recepción, mandar el proceso a EXIT
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        return;
    }
    
    // Procesar la respuesta
    if (respuesta == OK) {
        // Si la operación fue exitosa, desbloquear el proceso (pasa a READY)
        cambiar_estado_pcb(pcb_dump, READY);
        log_info(kernel_log, "## (%d) finalizó DUMP_MEMORY exitosamente y pasa a READY", pcb_dump->PID);
    } else {
        // Si hubo error, enviar el proceso a EXIT
        cambiar_estado_pcb(pcb_dump, EXIT_ESTADO);
        log_info(kernel_log, "## (%d) - Error en DUMP_MEMORY, proceso enviado a EXIT", pcb_dump->PID);
    }
}

//** Comento xq no se usa */
// // Declaraciones de funciones auxiliares
// static bool io_por_nombre_matcher(void* elemento, char* nombre);
// static bool pcb_io_matcher(void* elemento, io* disp, int pid);
// static bool esperando_mismo_io_matcher(void* elemento, io* disp);

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

    log_info(kernel_log, "\033[38;2;179;236;111m## (%d) - Bloqueado por IO: %s\033[0m", pcb_a_io->PID, nombre_io);
    log_debug(kernel_log, "## (%d) - Bloqueado por IO: %s (tiempo: %d ms)", pcb_a_io->PID, nombre_io, tiempo_a_usar);  

    cambiar_estado_pcb(pcb_a_io, BLOCKED);

    bloquear_pcb_por_io(nombre_io, pcb_a_io, tiempo_a_usar);
}

// Busca un dispositivo IO por su nombre
io* get_io(char* nombre_io) {
    io* dispositivo = NULL;
    
    if(nombre_io == NULL) {
        log_error(kernel_log, "get_io: Nombre de IO nulo");
        return NULL;
    }

    log_debug(kernel_log, "esperando mutex_ios para buscar IO por nombre '%s'", nombre_io);
    pthread_mutex_lock(&mutex_ios);
    log_debug(kernel_log, "bloqueando mutex_ios para buscar IO por nombre '%s'", nombre_io);

    for (int i = 0; i < list_size(lista_ios); i++) {
        io* io_actual = list_get(lista_ios, i);
        if (strcmp(io_actual->nombre, nombre_io) == 0) {
            dispositivo = io_actual;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_ios);

    log_trace(kernel_log, "get_io: IO '%s' %s encontrada", nombre_io, dispositivo ? "encontrada" : "no encontrada");

    return dispositivo;
}

io* buscar_io_por_fd(int fd) {
    if (!lista_ios) {
        log_error(kernel_log, "buscar_io_por_fd: lista_ios es NULL");
        return NULL;
    }

    // Buscar el dispositivo IO por file descriptor
    for (int i = 0; i < list_size(lista_ios); i++) {
        io* disp = list_get(lista_ios, i);
        if (disp && disp->fd == fd) {
            return disp;
        }
    }

    // No se encontró el dispositivo
    log_warning(kernel_log, "buscar_io_por_fd: No se encontró IO con fd %d", fd);
    return NULL;
}

io* buscar_io_por_nombre(char* nombre) {
    if (!lista_ios || !nombre) {
        log_error(kernel_log, "buscar_io_por_nombre: lista_ios o nombre es NULL");
        return NULL;
    }

    // Buscar el dispositivo IO por nombre
    for (int i = 0; i < list_size(lista_ios); i++) {
        io* disp = list_get(lista_ios, i);
        if (disp && strcmp(disp->nombre, nombre) == 0) {
            return disp;
        }
    }

    // No se encontró el dispositivo
    log_warning(kernel_log, "buscar_io_por_nombre: No se encontró IO con nombre '%s'", nombre);
    return NULL;
}

// Verifica si un dispositivo IO está disponible
bool esta_libre_io(io* dispositivo) {
    return dispositivo->estado == IO_DISPONIBLE;
}

io* io_disponible(char* nombre) {
    if (nombre == NULL) {
        log_error(kernel_log, "esta_libre_io: Nombre de IO nulo");
        return false;
    }
    log_debug(kernel_log, "esperando mutex_ios para verificar disponibilidad de IO '%s'", nombre);
    pthread_mutex_lock(&mutex_ios);
    log_debug(kernel_log, "bloqueando mutex_ios para verificar disponibilidad de IO '%s'", nombre);
    for(int i = 0; i < list_size(lista_ios); i++) {
        io* dispositivo = list_get(lista_ios, i);
        if (strcmp(dispositivo->nombre, nombre) == 0 && dispositivo->estado == IO_DISPONIBLE) {
            log_debug(kernel_log, "IO '%s' está disponible (fd=%d)", dispositivo->nombre, dispositivo->fd);
            pthread_mutex_unlock(&mutex_ios);
            return dispositivo;
        }
    }
    pthread_mutex_unlock(&mutex_ios);
    return NULL;
}

// Agrega un PCB a la lista de bloqueados por un dispositivo IO
void bloquear_pcb_por_io(char* nombre_io, t_pcb* pcb, int tiempo_a_usar) {
    io* dispositivo = io_disponible(nombre_io);
    if(dispositivo != NULL) {
        // Si la IO está disponible, se envía el proceso a la IO
        enviar_io(dispositivo, pcb, tiempo_a_usar);
    } else {
        // Si la IO no está disponible, se agrega a la lista de bloqueados por IO
        log_debug(kernel_log, "No hay IO disponible con el nombre '%s'", nombre_io);
        pthread_mutex_lock(&mutex_pcbs_esperando_io);
        t_pcb_io* pcb_io = malloc(sizeof(t_pcb_io));
        pcb_io->pcb = pcb;
        pcb_io->io = get_io(nombre_io);
        pcb_io->tiempo_a_usar = tiempo_a_usar;
        list_add(pcbs_esperando_io, pcb_io);
        pthread_mutex_unlock(&mutex_pcbs_esperando_io);
        log_trace(kernel_log, "PCB PID=%d agregado a la lista de bloqueados por IO '%s' por %d ms", 
                  pcb->PID, nombre_io, tiempo_a_usar);
    }
}

// Envía un proceso a un dispositivo IO
void enviar_io(io* dispositivo, t_pcb* pcb, int tiempo_a_usar) {
    dispositivo->estado = IO_OCUPADO;
    dispositivo->proceso_actual = pcb;

    int payload[2] = { pcb->PID, tiempo_a_usar };
    if (!enviar_operacion(dispositivo->fd, IO_OP) ||
        !enviar_enteros(dispositivo->fd, payload, 2)) {
        log_error(kernel_log, "Error al enviar IO_OP + datos a IO");
        dispositivo->estado = IO_DISPONIBLE;
        cambiar_estado_pcb(pcb, EXIT_ESTADO);
        return;
    }

    log_trace(kernel_log, "Enviado PID=%d a IO '%s' por %d ms", pcb->PID, dispositivo->nombre, tiempo_a_usar);
}

//////////////////////////////////////////////////////////// EXIT ////////////////////////////////////////////////////////////
void EXIT(t_pcb* pcb_a_finalizar) {
    if (!pcb_a_finalizar) {
        log_error(kernel_log, "EXIT: PCB nulo");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Notificar a Memoria
    int cod_op = FINALIZAR_PROC_OP;
    if (send(fd_memoria, &cod_op, sizeof(int), 0) <= 0 ||
        send(fd_memoria, &pcb_a_finalizar->PID, sizeof(int), 0) <= 0) {
        log_error(kernel_log, "EXIT: Error al enviar FINALIZAR_PROC_OP a Memoria para PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "EXIT: Enviado FINALIZAR_PROC_OP a Memoria para PID %d. Esperando respuesta...", pcb_a_finalizar->PID);
    t_respuesta confirmacion;
    if (recv(fd_memoria, &confirmacion, sizeof(t_respuesta), 0) <= 0) {
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
    log_info(kernel_log, "\033[38;2;179;236;111m## (%d) - Finaliza el proceso\033[0m", pcb_a_finalizar->PID);
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

    // Notificar a planificador LP
    sem_post(&sem_finalizacion_de_proceso);

    // Finalizar kernel cuando no haya más procesos
    log_debug(kernel_log, "EXIT: esperando mutex_cola_procesos para verificar si quedan procesos");
    pthread_mutex_lock(&mutex_cola_procesos);
    log_debug(kernel_log, "EXIT: bloqueando mutex_cola_procesos para verificar si quedan procesos");
    if(list_size(cola_procesos) == 0) {
        pthread_mutex_unlock(&mutex_cola_procesos);
        mostrar_colas_estados();
        log_info(kernel_log, "No quedan procesos en el sistema. Finalizando kernel...");
        terminar_kernel();
        exit(EXIT_SUCCESS);
    }
    pthread_mutex_unlock(&mutex_cola_procesos);
}
