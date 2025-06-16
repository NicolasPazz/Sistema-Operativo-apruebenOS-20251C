#include "../headers/syscalls.h"
#include "../headers/planificadores.h"

t_temporal* tiempo_estado_actual;

// Variable global para el siguiente PID
static int siguiente_pid = 1;

// Función para obtener el siguiente PID disponible
static int obtener_siguiente_pid() {
    return siguiente_pid++;
}

//////////////////////////////////////////////////////////// INIT PROC ////////////////////////////////////////////////////////////
void INIT_PROC(char* nombre_archivo, int tam_memoria) {
    log_info(kernel_log, "## Solicitó syscall: INIT_PROC");
    log_debug(kernel_log, "INIT_PROC - Nombre archivo recibido: '%s'", nombre_archivo);
    
    // Crear nuevo PCB
    t_pcb* nuevo_proceso = malloc(sizeof(t_pcb));
    nuevo_proceso->PID = obtener_siguiente_pid();
    nuevo_proceso->Estado = NEW;
    nuevo_proceso->tamanio_memoria = tam_memoria;
    nuevo_proceso->path = strdup(nombre_archivo);
    nuevo_proceso->PC = 0;  // Inicializar PC a 0
    
    // Comunicarse con memoria para inicializar el proceso
    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
    agregar_a_paquete(paquete, &nuevo_proceso->PID, sizeof(int));
    
    // Asegurarnos de que el nombre del archivo se envíe correctamente
    log_debug(kernel_log, "INIT_PROC - Nombre archivo a enviar: '%s'", nombre_archivo);
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);
    
    agregar_a_paquete(paquete, &tam_memoria, sizeof(int));
    
    // Usar la conexión global fd_memoria en lugar de crear una nueva
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
    
    // Esperar respuesta de memoria
    t_respuesta_memoria respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta_memoria), 0) <= 0) {
        log_error(kernel_log, "Error al recibir respuesta de memoria para INIT_PROC");
        free(nuevo_proceso->path);
        free(nuevo_proceso);
        return;
    }
    
    // Procesar respuesta
    if (respuesta == OK) {
        // Agregar proceso a la cola NEW
        pthread_mutex_lock(&mutex_cola_new);
        list_add(cola_new, nuevo_proceso);
        pthread_mutex_unlock(&mutex_cola_new);
        
        // Notificar al planificador de largo plazo
        sem_post(&sem_proceso_a_new);
        
        log_info(kernel_log, "## (%d) Se crea el proceso - Estado: NEW", nuevo_proceso->PID);
    } else {
        log_error(kernel_log, "Error al crear proceso en memoria");
        free(nuevo_proceso->path);
        free(nuevo_proceso);
    }
}

//////////////////////////////////////////////////////////// DUMP MEMORY ////////////////////////////////////////////////////////////
void DUMP_MEMORY(){
    
}

// Declaraciones de funciones auxiliares
static bool io_por_nombre_matcher(void* elemento, char* nombre);
static bool pcb_io_matcher(void* elemento, io* disp, int pid);
static bool esperando_mismo_io_matcher(void* elemento, io* disp);

// Variables externas
extern t_list* lista_ios;
extern pthread_mutex_t mutex_ios;
extern t_list* pcbs_bloqueados_por_io;

//////////////////////////////////////////////////////////// IO ////////////////////////////////////////////////////////////

// Busca un dispositivo IO por su nombre
io* get_io(char* nombre_io) {
    pthread_mutex_lock(&mutex_ios);
    io* dispositivo = NULL;
    
    // Buscar manualmente
    for (int i = 0; i < list_size(lista_ios); i++) {
        io* io_actual = list_get(lista_ios, i);
        if (strcmp(io_actual->nombre, nombre_io) == 0) {
            dispositivo = io_actual;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_ios);
    return dispositivo;
}

// Verifica si un dispositivo IO está disponible
bool esta_libre_io(io* dispositivo) {
    return dispositivo->estado == IO_DISPONIBLE;
}

// Agrega un PCB a la lista de bloqueados por un dispositivo IO
void bloquear_pcb_por_io(io* dispositivo, t_pcb* pcb, int tiempo_a_usar) {
    // Crear un registro de PCB bloqueado por IO
    t_pcb_io* pcb_io = malloc(sizeof(t_pcb_io));
    if (!pcb_io) {
        log_error(kernel_log, "Error al reservar memoria para PCB_IO");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    
    pcb_io->pcb = pcb;
    pcb_io->io = dispositivo;
    pcb_io->tiempo_a_usar = tiempo_a_usar;  // Guardar el tiempo de ejecución
    
    // Agregar a la lista de PCBs bloqueados
    list_add(pcbs_bloqueados_por_io, pcb_io);
    
    log_info(kernel_log, "## (%d) - Bloqueado por IO: %s (tiempo: %d ms)", 
             pcb->PID, dispositivo->nombre, tiempo_a_usar);
}

// Envía un proceso a un dispositivo IO
void enviar_io(io* dispositivo, t_pcb* pcb, int tiempo_a_usar) {
    // Marcar el dispositivo como ocupado
    dispositivo->estado = IO_OCUPADO;
    
    // Enviar PID y tiempo a usar a la IO
    op_code cod_op = IO_OP;
    if (send(dispositivo->fd, &cod_op, sizeof(op_code), 0) <= 0) {
        log_error(kernel_log, "Error al enviar IO_OP a IO '%s'", dispositivo->nombre);
        dispositivo->estado = IO_DISPONIBLE;
        cambiar_estado_pcb(pcb, EXIT_ESTADO);
        return;
    }
    
    // Enviar PID y tiempo
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = IO_OP;
    
    // Agregar PID y tiempo al paquete
    agregar_a_paquete(paquete, &pcb->PID, sizeof(int));
    agregar_a_paquete(paquete, &tiempo_a_usar, sizeof(int));
    
    // Enviar paquete
    enviar_paquete(paquete, dispositivo->fd);
    eliminar_paquete(paquete);
    
    log_info(kernel_log, "Enviado PID=%d a IO '%s' por %d ms", pcb->PID, dispositivo->nombre, tiempo_a_usar);
}

// Procesa una solicitud de entrada/salida
void IO(char* nombre_io, int tiempo_a_usar, t_pcb* pcb_a_io) {
    if (!pcb_a_io) {
        log_error(kernel_log, "IO: PCB nulo");
        return;
    }
    
    log_info(kernel_log, "## (%d) - Solicitó syscall: IO", pcb_a_io->PID);
    
    // Obtener el dispositivo IO
    io* dispositivo = get_io(nombre_io);
    
    // Validar que la IO solicitada existe en el sistema
    if (dispositivo == NULL) {
        log_error(kernel_log, "IO: No existe el dispositivo '%s'", nombre_io);
        cambiar_estado_pcb(pcb_a_io, EXIT_ESTADO);
        return;
    }
    
    // Cambiar estado del proceso a BLOCKED
    cambiar_estado_pcb(pcb_a_io, BLOCKED);
    
    // Bloquear el proceso por la IO
    bloquear_pcb_por_io(dispositivo, pcb_a_io, tiempo_a_usar);
    
    // Si la IO está libre, enviar el proceso
    if (esta_libre_io(dispositivo)) {
        enviar_io(dispositivo, pcb_a_io, tiempo_a_usar);
    } else {
        log_info(kernel_log, "IO '%s' ocupada, proceso PID=%d en espera", nombre_io, pcb_a_io->PID);
    }
}

// Procesa la finalización de una operación IO
void fin_io(io* dispositivo, int pid_finalizado) {
    t_pcb_io* pcb_io = NULL;
    
    // Buscar manualmente el PCB bloqueado por esta IO con este PID
    for (int i = 0; i < list_size(pcbs_bloqueados_por_io); i++) {
        t_pcb_io* pcb_io_actual = list_get(pcbs_bloqueados_por_io, i);
        if (pcb_io_actual->io == dispositivo && pcb_io_actual->pcb->PID == pid_finalizado) {
            pcb_io = list_remove(pcbs_bloqueados_por_io, i);
            break;
        }
    }
    
    if (!pcb_io) {
        log_error(kernel_log, "fin_io: No se encontró PCB para PID=%d en IO '%s'", 
                 pid_finalizado, dispositivo->nombre);
        return;
    }
    
    t_pcb* pcb = pcb_io->pcb;
    free(pcb_io);
    
    // Cambiar estado del proceso a READY
    cambiar_estado_pcb(pcb, READY);
    log_info(kernel_log, "## (%d) finalizó IO y pasa a READY", pcb->PID);
    
    // Marcar la IO como disponible
    dispositivo->estado = IO_DISPONIBLE;
    
    // Buscar el siguiente proceso que espera esta IO
    t_pcb_io* siguiente_pcb_io = NULL;
    for (int i = 0; i < list_size(pcbs_bloqueados_por_io); i++) {
        t_pcb_io* pcb_io_actual = list_get(pcbs_bloqueados_por_io, i);
        if (pcb_io_actual->io == dispositivo) {
            siguiente_pcb_io = pcb_io_actual;
            break;
        }
    }
    
    // Si hay un proceso esperando, enviarlo a la IO
    if (siguiente_pcb_io) {
        // Usar el tiempo guardado en la estructura
        log_info(kernel_log, "Enviando siguiente proceso PID=%d a IO '%s' por %d ms", 
                siguiente_pcb_io->pcb->PID, dispositivo->nombre, siguiente_pcb_io->tiempo_a_usar);
        enviar_io(dispositivo, siguiente_pcb_io->pcb, siguiente_pcb_io->tiempo_a_usar);
    }
}

// Implementaciones de funciones auxiliares
bool io_por_nombre_matcher(void* elemento, char* nombre) {
    io* dispositivo = (io*) elemento;
    return strcmp(dispositivo->nombre, nombre) == 0;
}

bool pcb_io_matcher(void* elemento, io* disp, int pid) {
    t_pcb_io* pcb_io = (t_pcb_io*) elemento;
    return pcb_io->io == disp && pcb_io->pcb->PID == pid;
}

bool esperando_mismo_io_matcher(void* elemento, io* disp) {
    t_pcb_io* pcb_io = (t_pcb_io*) elemento;
    return pcb_io->io == disp;
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

    t_respuesta_memoria confirmacion;
    if (recv(fd_memoria, &confirmacion, sizeof(t_respuesta_memoria), 0) <= 0) {
        log_error(kernel_log, "EXIT: No se pudo recibir confirmación de Memoria para PID %d", pcb_a_finalizar->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    
    if (confirmacion == OK) {
        log_debug(kernel_log, "EXIT: Memoria confirmó finalización de PID %d", pcb_a_finalizar->PID);
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
    log_info(kernel_log, "## (<%d>) - Finaliza el proceso", pcb_a_finalizar->PID);
    loguear_metricas_estado(pcb_a_finalizar);

    // Eliminar de cola_exit, liberar pcb y cronometro
    pthread_mutex_lock(&mutex_cola_exit);
    list_remove_element(cola_exit, pcb_a_finalizar);
    pthread_mutex_unlock(&mutex_cola_exit);

    char* pid_key = string_itoa(pcb_a_finalizar->PID);
    dictionary_remove_and_destroy(tiempos_por_pid, pid_key, (void*) temporal_destroy);
    free(pid_key);

    free(pcb_a_finalizar->path);
    free(pcb_a_finalizar);

    // Notificar a planificador LP
    sem_post(&sem_finalizacion_de_proceso);
}
