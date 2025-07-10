#include "../headers/IOKernel.h"

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
            log_trace(kernel_log, "IO '%s' está disponible (fd=%d)", dispositivo->nombre, dispositivo->fd);
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
        log_trace(kernel_log, "No hay IO disponible con el nombre '%s'", nombre_io);
        log_debug(kernel_log, "bloquear_pcb_por_io: esperando mutex_pcbs_esperando_io para agregar PCB PID=%d a la lista de bloqueados por IO '%s' por %d ms", 
                  pcb->PID, nombre_io, tiempo_a_usar);
        pthread_mutex_lock(&mutex_pcbs_esperando_io);
        log_debug(kernel_log, "bloquear_pcb_por_io: bloqueando mutex_pcbs_esperando_io para agregar PCB PID=%d a la lista de bloqueados por IO '%s' por %d ms", 
                  pcb->PID, nombre_io, tiempo_a_usar);
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

    // Crear paquete serializado
    t_paquete* paquete = crear_paquete_op(IO_OP);
    agregar_a_paquete(paquete, dispositivo->nombre, strlen(dispositivo->nombre) + 1); // nombre de la IO
    agregar_entero_a_paquete(paquete, tiempo_a_usar); // tiempo
    agregar_entero_a_paquete(paquete, pcb->PID); // pid

    enviar_paquete(paquete, dispositivo->fd);
    eliminar_paquete(paquete);

    log_trace(kernel_log, "Enviado PID=%d a IO '%s' por %d ms", pcb->PID, dispositivo->nombre, tiempo_a_usar);
}

