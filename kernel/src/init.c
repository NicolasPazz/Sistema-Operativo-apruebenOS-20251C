#include "../headers/kernel.h"

/////////////////////////////// Declaracion de variables globales ///////////////////////////////
// Logger
t_log* kernel_log;
t_log* kernel_log_debug;

// Hashmap de cronometros por PCB
t_dictionary* tiempos_por_pid;
t_dictionary* archivo_por_pcb;

// Sockets
int fd_dispatch;
int fd_cpu_dispatch;
int fd_interrupt;
int fd_cpu_interrupt;
int fd_memoria;
int fd_kernel_io;
int fd_io;

// Config
t_config* kernel_config;
char* IP_MEMORIA;
char* PUERTO_MEMORIA;
char* PUERTO_ESCUCHA_DISPATCH;
char* PUERTO_ESCUCHA_INTERRUPT;
char* PUERTO_ESCUCHA_IO;
char* ALGORITMO_CORTO_PLAZO;
char* ALGORITMO_INGRESO_A_READY;
char* ALFA;
char* TIEMPO_SUSPENSION;
char* ESTIMACION_INICIAL;
char* LOG_LEVEL;

// Colas de Estados
t_list* cola_new;
t_list* cola_ready;
t_list* cola_running;
t_list* cola_blocked;
t_list* cola_susp_ready;
t_list* cola_susp_blocked;
t_list* cola_exit;
t_list* cola_procesos; // Cola con TODOS los procesos sin importar el estado (Procesos totales del sistema)
t_list* pcbs_bloqueados_por_io;

// Listas y semaforos de CPUs y IOs conectadas
t_list* lista_cpus;
pthread_mutex_t mutex_lista_cpus;
t_list* lista_ios;
pthread_mutex_t mutex_ios;

// Conexiones minimas
bool conectado_cpu = false;
bool conectado_io = false;
pthread_mutex_t mutex_conexiones;

// Semaforos de planificacion
pthread_mutex_t mutex_cola_new;
pthread_mutex_t mutex_cola_susp_ready;
pthread_mutex_t mutex_cola_susp_blocked;
pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_running;
pthread_mutex_t mutex_cola_blocked;
pthread_mutex_t mutex_cola_exit;
sem_t sem_proceso_a_new;
sem_t sem_proceso_a_susp_ready;
sem_t sem_proceso_a_susp_blocked;
sem_t sem_proceso_a_ready;
sem_t sem_proceso_a_running;
sem_t sem_proceso_a_blocked;
sem_t sem_proceso_a_exit;
sem_t sem_susp_ready_vacia;
sem_t sem_finalizacion_de_proceso;

/////////////////////////////// Inicializacion de variables globales ///////////////////////////////
void iniciar_config_kernel() {
    kernel_config = iniciar_config("kernel.config");

    IP_MEMORIA = config_get_string_value(kernel_config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_string_value(kernel_config, "PUERTO_MEMORIA");
    PUERTO_ESCUCHA_DISPATCH = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_DISPATCH");
    PUERTO_ESCUCHA_INTERRUPT = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_INTERRUPT");
    PUERTO_ESCUCHA_IO = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_IO");
    ALGORITMO_CORTO_PLAZO = config_get_string_value(kernel_config, "ALGORITMO_CORTO_PLAZO");
    ALGORITMO_INGRESO_A_READY = config_get_string_value(kernel_config, "ALGORITMO_INGRESO_A_READY");
    ALFA = config_get_string_value(kernel_config, "ALFA");
    ESTIMACION_INICIAL = config_get_string_value(kernel_config, "ESTIMACION_INICIAL");
    TIEMPO_SUSPENSION = config_get_string_value(kernel_config, "TIEMPO_SUSPENSION");
    LOG_LEVEL = config_get_string_value(kernel_config, "LOG_LEVEL");

    if (!IP_MEMORIA || !PUERTO_MEMORIA || !PUERTO_ESCUCHA_DISPATCH || !PUERTO_ESCUCHA_INTERRUPT ||
        !PUERTO_ESCUCHA_IO || !ALGORITMO_CORTO_PLAZO || !ALGORITMO_INGRESO_A_READY ||
        !ALFA || !ESTIMACION_INICIAL || !TIEMPO_SUSPENSION || !LOG_LEVEL) {
        log_error(kernel_log_debug, "iniciar_config_kernel: Faltan campos obligatorios en kernel.config");
        exit(EXIT_FAILURE);
    } else {
        log_debug(kernel_log_debug, "IP_MEMORIA: %s", IP_MEMORIA);
        log_debug(kernel_log_debug, "PUERTO_MEMORIA: %s", PUERTO_MEMORIA);
        log_debug(kernel_log_debug, "PUERTO_ESCUCHA_DISPATCH: %s", PUERTO_ESCUCHA_DISPATCH);
        log_debug(kernel_log_debug, "PUERTO_ESCUCHA_INTERRUPT: %s", PUERTO_ESCUCHA_INTERRUPT);
        log_debug(kernel_log_debug, "PUERTO_ESCUCHA_IO: %s", PUERTO_ESCUCHA_IO);
        log_debug(kernel_log_debug, "ALGORITMO_CORTO_PLAZO: %s", ALGORITMO_CORTO_PLAZO);
        log_debug(kernel_log_debug, "ALGORITMO_INGRESO_A_READY: %s", ALGORITMO_INGRESO_A_READY);
        log_debug(kernel_log_debug, "ALFA: %s", ALFA);
        log_debug(kernel_log_debug, "ESTIMACION_INICIAL: %s", ESTIMACION_INICIAL);
        log_debug(kernel_log_debug, "TIEMPO_SUSPENSION: %s", TIEMPO_SUSPENSION);
        log_debug(kernel_log_debug, "LOG_LEVEL: %s", LOG_LEVEL);
    }
}

void iniciar_logger_kernel() {
    kernel_log = iniciar_logger("kernel.log", "kernel", 1, log_level_from_string(LOG_LEVEL));
    log_debug(kernel_log, "Kernel log iniciado correctamente!");
}

void iniciar_logger_kernel_debug() {
    kernel_log_debug = iniciar_logger("kernel_config_debug.log", "kernel", 1, LOG_LEVEL_TRACE);
    log_debug(kernel_log_debug, "Kernel log de debug iniciado correctamente!");
}

void iniciar_estados_kernel(){ 
    cola_new = list_create();
    cola_ready = list_create();
    cola_running = list_create();
    cola_blocked = list_create();
    cola_susp_ready = list_create();
    cola_susp_blocked = list_create();
    cola_exit = list_create();
    cola_procesos = list_create();
    pcbs_bloqueados_por_io = list_create();
}

void iniciar_sincronizacion_kernel() {
    pthread_mutex_init(&mutex_lista_cpus, NULL);
    pthread_mutex_init(&mutex_ios, NULL);
    pthread_mutex_init(&mutex_conexiones, NULL);

    pthread_mutex_init(&mutex_cola_new, NULL);
    pthread_mutex_init(&mutex_cola_susp_ready, NULL);
    pthread_mutex_init(&mutex_cola_susp_blocked, NULL);
    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_running, NULL);
    pthread_mutex_init(&mutex_cola_blocked, NULL);
    pthread_mutex_init(&mutex_cola_exit, NULL);

    sem_init(&sem_proceso_a_new, 0, 0);
    sem_init(&sem_proceso_a_susp_ready, 0, 0);
    sem_init(&sem_proceso_a_susp_blocked, 0, 0);
    sem_init(&sem_proceso_a_ready, 0, 0);
    sem_init(&sem_proceso_a_running, 0, 0);
    sem_init(&sem_proceso_a_blocked, 0, 0);
    sem_init(&sem_proceso_a_exit, 0, 0);
    sem_init(&sem_susp_ready_vacia, 0, 1);
    sem_init(&sem_finalizacion_de_proceso, 0, 0);
    
    lista_cpus = list_create();
    lista_ios = list_create();

    conectado_cpu = false;
    conectado_io = false;
}

void iniciar_diccionario_tiempos() {
    tiempos_por_pid = dictionary_create();
}

void iniciar_diccionario_archivos_por_pcb() {
    archivo_por_pcb = dictionary_create();
}

void terminar_kernel(){
    log_destroy(kernel_log);
    log_destroy(kernel_log_debug);
    config_destroy(kernel_config);

    list_destroy(cola_new);
    list_destroy(cola_ready);
    list_destroy(cola_running);
    list_destroy(cola_blocked);
    list_destroy(cola_susp_ready);
    list_destroy(cola_susp_blocked);
    list_destroy(cola_exit);
    list_destroy(cola_procesos);
    list_destroy(pcbs_bloqueados_por_io);

    pthread_mutex_destroy(&mutex_lista_cpus);
    pthread_mutex_destroy(&mutex_ios);
    pthread_mutex_destroy(&mutex_conexiones);
    pthread_mutex_destroy(&mutex_cola_new);
    pthread_mutex_destroy(&mutex_cola_susp_ready);
    pthread_mutex_destroy(&mutex_cola_susp_blocked);
    pthread_mutex_destroy(&mutex_cola_ready);
    pthread_mutex_destroy(&mutex_cola_running);
    pthread_mutex_destroy(&mutex_cola_blocked);
    pthread_mutex_destroy(&mutex_cola_exit);

    sem_destroy(&sem_proceso_a_new);
    sem_destroy(&sem_proceso_a_susp_ready);
    sem_destroy(&sem_proceso_a_susp_blocked);
    sem_destroy(&sem_proceso_a_ready);
    sem_destroy(&sem_proceso_a_running);
    sem_destroy(&sem_proceso_a_blocked);
    sem_destroy(&sem_proceso_a_exit);
    sem_destroy(&sem_susp_ready_vacia);
    sem_destroy(&sem_finalizacion_de_proceso);

    list_destroy(lista_cpus);
    list_destroy(lista_ios);

    close(fd_cpu_dispatch);
    close(fd_cpu_interrupt);
    close(fd_memoria);
    close(fd_kernel_io);
    close(fd_io);
    close(fd_dispatch);
    close(fd_interrupt);
}

//////////////////////////// Conexiones del Kernel ////////////////////////////
void* hilo_cliente_memoria(void* _){
    ////////// Conexion hacia Memoria //////////
    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, kernel_log);

    if (fd_memoria == -1) {
        log_error(kernel_log, "Error al conectar Kernel a Memoria");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    int handshake = HANDSHAKE_MEMORIA_KERNEL;
    if (send(fd_memoria, &handshake, sizeof(int), 0) <= 0) {
        log_error(kernel_log, "Error al enviar handshake a Memoria");
        close(fd_memoria);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_debug(kernel_log, "HANDSHAKE_MEMORIA_KERNEL: Kernel conectado correctamente a Memoria (fd=%d)", fd_memoria);

    return NULL;
}

void* hilo_servidor_dispatch(void* _) {
    ////////// Servidor Dispatch escuchando conexiones  //////////
    fd_dispatch = iniciar_servidor(PUERTO_ESCUCHA_DISPATCH, kernel_log, "Servidor Kernel Dispatch");

    while (1) {
        int fd_cpu_dispatch = esperar_cliente(fd_dispatch, kernel_log);
        if (fd_cpu_dispatch == -1) {
            log_error(kernel_log, "hilo_servidor_dispatch: Error al recibir cliente");
            continue;
        }

        if (!validar_handshake(fd_cpu_dispatch, HANDSHAKE_CPU_KERNEL_DISPATCH, kernel_log)) {
            close(fd_cpu_dispatch);
            continue;
        }

        uint32_t id_cpu;
        if (recv(fd_cpu_dispatch, &id_cpu, sizeof(uint32_t), 0) <= 0) {
            log_error(kernel_log, "Error al recibir ID de CPU desde Dispatch");
            close(fd_cpu_dispatch);
            continue;
        }

        cpu* nueva_cpu = malloc(sizeof(cpu));
        nueva_cpu->fd = fd_cpu_dispatch;
        nueva_cpu->id = id_cpu;
        nueva_cpu->tipo_conexion = CPU_DISPATCH;

        pthread_mutex_lock(&mutex_lista_cpus);
        list_add(lista_cpus, nueva_cpu);
        pthread_mutex_unlock(&mutex_lista_cpus);

        pthread_mutex_lock(&mutex_conexiones);
        conectado_cpu = true;
        pthread_mutex_unlock(&mutex_conexiones);

        log_debug(kernel_log, "HANDSHAKE_CPU_KERNEL_DISPATCH: CPU conectada exitosamente a Dispatch (fd=%d), ID=%d", nueva_cpu->fd, nueva_cpu->id);

        int* arg = malloc(sizeof(int));
        *arg = fd_cpu_dispatch;
        pthread_t hilo;
        pthread_create(&hilo, NULL, atender_cpu_dispatch, arg);
        pthread_detach(hilo);
    }

    return NULL;
}

void* hilo_servidor_interrupt(void* _){
    ////////// Servidor Interrupt escuchando conexiones  //////////
    fd_interrupt = iniciar_servidor(PUERTO_ESCUCHA_INTERRUPT, kernel_log, "Servidor Kernel Interrupt");

    while (1) {
        int fd_cpu_interrupt = esperar_cliente(fd_interrupt, kernel_log);
        if (fd_cpu_interrupt == -1) {
            log_error(kernel_log, "hilo_servidor_interrupt: Error al recibir cliente");
            continue;
        }

        if (!validar_handshake(fd_cpu_interrupt, HANDSHAKE_CPU_KERNEL_INTERRUPT, kernel_log)) {
            close(fd_cpu_interrupt);
            continue;
        }

        uint32_t id_cpu;
        if (recv(fd_cpu_interrupt, &id_cpu, sizeof(uint32_t), 0) <= 0) {
            log_error(kernel_log, "Error al recibir ID de CPU desde Interrupt");
            close(fd_cpu_interrupt);
            continue;
        }

        cpu* nueva_cpu = malloc(sizeof(cpu));
        nueva_cpu->fd = fd_cpu_interrupt;
        nueva_cpu->id = id_cpu;
        nueva_cpu->tipo_conexion = CPU_INTERRUPT;
        
        pthread_mutex_lock(&mutex_lista_cpus);
        list_add(lista_cpus, nueva_cpu);
        pthread_mutex_unlock(&mutex_lista_cpus);

        pthread_mutex_lock(&mutex_conexiones);
        conectado_cpu = true;
        pthread_mutex_unlock(&mutex_conexiones);

        log_debug(kernel_log, "HANDSHAKE_CPU_KERNEL_INTERRUPT: CPU conectada exitosamente a Interrupt (fd=%d), ID=%d", nueva_cpu->fd, nueva_cpu->id);

        int* arg = malloc(sizeof(int));
        *arg = fd_cpu_interrupt;
        pthread_t hilo;
        pthread_create(&hilo, NULL, atender_cpu_interrupt, arg);
        pthread_detach(hilo);
    }

    return NULL;
}

void* hilo_servidor_io(void* _){
    ////////// Servidor IO //////////
    fd_kernel_io = iniciar_servidor(PUERTO_ESCUCHA_IO, kernel_log, "Servidor Kernel IO");

    while (1) {
        int fd_io = esperar_cliente(fd_kernel_io, kernel_log);
        if (fd_io == -1) {
            log_error(kernel_log, "hilo_servidor_io: Error al recibir cliente");
            continue;
        }

        if (!validar_handshake(fd_io, HANDSHAKE_IO_KERNEL, kernel_log)) {
            close(fd_io);
            continue;
        }

        char nombre_io[256];
        if (recv(fd_io, nombre_io, sizeof(nombre_io), 0) <= 0) {
            log_error(kernel_log, "Error al recibir nombre de IO");
            close(fd_io);
            continue;
        }

        io* nueva_io = malloc(sizeof(io));
        nueva_io->fd = fd_io;
        nueva_io->nombre = strdup(nombre_io); 
        nueva_io->estado = IO_DISPONIBLE;

        pthread_mutex_lock(&mutex_ios);
        list_add(lista_ios, nueva_io);
        pthread_mutex_unlock(&mutex_ios);

        pthread_mutex_lock(&mutex_conexiones);
        conectado_io = true;
        pthread_mutex_unlock(&mutex_conexiones);

        log_debug(kernel_log, "HANDSHAKE_IO_KERNEL: IO '%s' conectada exitosamente (fd=%d)", nueva_io->nombre, fd_io);

        int* arg = malloc(sizeof(int));
        *arg = fd_io;
        pthread_t hilo;
        pthread_create(&hilo, NULL, atender_io, arg);
        pthread_detach(hilo);
    }

    return NULL;
}

<<<<<<< HEAD
bool cpu_por_fd(void* ptr) {
    cpu* c = (cpu*) ptr;
    return c->fd == fd_cpu_dispatch;
}

/*
bool cpu_por_fd(void* ptr, int fd) {
    cpu* c = (cpu*) ptr;
    return c->fd == fd;
}*/
=======
// Funciones auxiliares para buscar CPU por file descriptor
bool cpu_por_fd_simple(void* ptr, int fd) {
    cpu* c = (cpu*) ptr;
    return c->fd == fd;
}

// Encuentra la CPU por su fd y devuelve el PID del proceso que está ejecutando
uint16_t get_pid_from_cpu(int fd, op_code instruccion) {
    pthread_mutex_lock(&mutex_lista_cpus);

    // Buscar por fd e instrucción
    cpu* cpu_asociada = NULL;
    for (int i = 0; i < list_size(lista_cpus); i++) {
        cpu* c = list_get(lista_cpus, i);
        if (c->fd == fd && c->instruccion_actual == instruccion) {
            cpu_asociada = c;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_lista_cpus);

    if (!cpu_asociada) {
        log_error(kernel_log, "No se encontró CPU asociada a fd=%d con instrucción=%d", fd, instruccion);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    return cpu_asociada->pid;
}
>>>>>>> b1f878b7fcd43952b9a081935d83829ccfc219cc

void* atender_cpu_dispatch(void* arg) {
    int fd_cpu_dispatch = *(int*)arg;
    free(arg);

    op_code cop;
    while (recv(fd_cpu_dispatch, &cop, sizeof(op_code), 0) > 0) {
        // Asignar la instrucción actual a la CPU y asociar el PID
        pthread_mutex_lock(&mutex_lista_cpus);
        
        // Buscar CPU por fd
        cpu* cpu_actual = NULL;
        for (int i = 0; i < list_size(lista_cpus); i++) {
            cpu* c = list_get(lista_cpus, i);
            if (c->fd == fd_cpu_dispatch) {
                cpu_actual = c;
                break;
            }
        }
        
        if (cpu_actual) {
            // Actualizar la operación actual que está procesando esta CPU
            cpu_actual->instruccion_actual = cop;
            log_debug(kernel_log, "CPU ID=%d está procesando operación %d", cpu_actual->id, cop);
        } else {
            log_error(kernel_log, "No se encontró la CPU con fd=%d en la lista", fd_cpu_dispatch);
            pthread_mutex_unlock(&mutex_lista_cpus);
            break;
        }
        pthread_mutex_unlock(&mutex_lista_cpus);

        switch (cop) {
            case IO_OP:
                log_debug(kernel_log, "IO_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);

                // Recibir el nombre_io y cant_tiempo desde CPU
                char* nombre_IO = NULL;
                uint16_t cant_tiempo;
                if (recv_IO_from_CPU(fd_cpu_dispatch, &nombre_IO, &cant_tiempo)) {
                    log_info(kernel_log, "Se recibió correctamente la IO '%s' desde CPU, tiempo=%d", 
                             nombre_IO, cant_tiempo);
                    
                    // Obtener PID del proceso que está ejecutando esta CPU
                    pthread_mutex_lock(&mutex_lista_cpus);
                    uint16_t pid = cpu_actual->pid;
                    pthread_mutex_unlock(&mutex_lista_cpus);
                    
                    log_debug(kernel_log, "IO_OP asociado a PID=%d", pid);
                    
                    // Obtener PCB por PID
                    t_pcb* pcb_a_io = NULL;
                    for (int i = 0; i < list_size(cola_procesos); i++) {
                        t_pcb* pcb = list_get(cola_procesos, i);
                        if (pcb->PID == pid) {
                            pcb_a_io = pcb;
                            break;
                        }
                    }
                    
                    if (pcb_a_io) {
                        // Exec Syscall: IO
                        procesar_IO_from_CPU(nombre_IO, cant_tiempo, pcb_a_io);
                    } else {
                        log_error(kernel_log, "No se encontró PCB para PID=%d", pid);
                    }
                    
                    free(nombre_IO);
                } else {
                    log_error(kernel_log, "Error al recibir la IO desde CPU");
                }

<<<<<<< HEAD
                uint16_t pid_en_cpu = get_pid_from_cpu(fd_cpu_dispatch, IO_OP);
                log_debug(kernel_log, "IO_OP asociado a PID=%d", pid_en_cpu);

                // Exec Syscall: IO
                t_pcb* pcb_a_io = list_get(cola_procesos, pid_en_cpu);
                procesar_IO_from_CPU(nombre_IO, cant_tiempo, pcb_a_io);

=======
>>>>>>> b1f878b7fcd43952b9a081935d83829ccfc219cc
                break;

            case EXIT_OP:
                log_debug(kernel_log, "EXIT_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);

                // Obtener PID del proceso que está ejecutando esta CPU
                pthread_mutex_lock(&mutex_lista_cpus);
                uint16_t pid = cpu_actual->pid;
                pthread_mutex_unlock(&mutex_lista_cpus);
                
                log_debug(kernel_log, "EXIT_OP asociado a PID=%d", pid);

                // Buscar PCB en RUNNING
                pthread_mutex_lock(&mutex_cola_running);
                t_pcb* pcb_a_finalizar = NULL;
                for (int i = 0; i < list_size(cola_running); i++) {
                    t_pcb* pcb = list_get(cola_running, i);
                    if (pcb->PID == pid) {
                        pcb_a_finalizar = list_remove(cola_running, i);
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex_cola_running);

                // Confirmar que se encontró el PCB
                if (pcb_a_finalizar) {
                    // Cambiar estado y finalizar
                    cambiar_estado_pcb(pcb_a_finalizar, EXIT_ESTADO);
                } else {
                    log_error(kernel_log, "EXIT: No se encontró PCB para PID=%d en RUNNING", pid);
                }

                // Limpiar PID de la CPU asociada
                pthread_mutex_lock(&mutex_lista_cpus);
                cpu_actual->pid = -1; // Limpiar PID de la CPU
                pthread_mutex_unlock(&mutex_lista_cpus);
                
                break;

            case DUMP_MEMORY_OP:
                log_debug(kernel_log, "DUMP_MEMORY_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);
                // TODO: Implementar DUMP_MEMORY
                break;
                
            default:
                log_error(kernel_log, "Código op desconocido recibido de Dispatch: %d", cop);
                break;
        }

        // Limpiar la instrucción actual de la CPU
        pthread_mutex_lock(&mutex_lista_cpus);
        cpu_actual->instruccion_actual = -1; // Valor inválido para indicar que está libre
        pthread_mutex_unlock(&mutex_lista_cpus);
    }

    log_warning(kernel_log, "CPU Dispatch desconectada (fd=%d)", fd_cpu_dispatch);
<<<<<<< HEAD
=======
    
    // Eliminar esta CPU de la lista de CPUs
    pthread_mutex_lock(&mutex_lista_cpus);
    for (int i = 0; i < list_size(lista_cpus); i++) {
        cpu* c = list_get(lista_cpus, i);
        if (c->fd == fd_cpu_dispatch) {
            cpu* cpu_eliminada = list_remove(lista_cpus, i);
            free(cpu_eliminada);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_lista_cpus);
    
>>>>>>> b1f878b7fcd43952b9a081935d83829ccfc219cc
    close(fd_cpu_dispatch);
    return NULL;
}

void* atender_cpu_interrupt(void* arg) {
    int fd_cpu_interrupt = *(int*)arg;
    free(arg);

    op_code cop;
    while (recv(fd_cpu_interrupt, &cop, sizeof(op_code), 0) > 0) {
        switch (cop) {
            default:
                log_error(kernel_log, "Codigo op desconocido recibido de Interrupt (fd=%d): %d", fd_cpu_interrupt, cop);
                terminar_kernel();
                exit(EXIT_FAILURE);
        }
    }

    log_warning(kernel_log, "CPU Interrupt desconectada (fd=%d)", fd_cpu_interrupt);
    close(fd_cpu_interrupt);
    return NULL;
}

// Declaraciones de funciones auxiliares
static bool es_bloqueado_por_io(void* elemento, io* dispositivo_io);
static bool es_misma_instancia(void* e, t_pcb_io* instancia);
static bool es_misma_io(void* elemento, io* disp_io);

void* atender_io(void* arg) {
    int fd_io = *(int*)arg;
    free(arg);

    // Encontrar la IO asociada a este file descriptor
    io* dispositivo_io = NULL;
    pthread_mutex_lock(&mutex_ios);
    for (int i = 0; i < list_size(lista_ios); i++) {
        io* disp = list_get(lista_ios, i);
        if (disp->fd == fd_io) {
            dispositivo_io = disp;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_ios);

    if (!dispositivo_io) {
        log_error(kernel_log, "atender_io: No se encontró IO con fd=%d", fd_io);
        close(fd_io);
        return NULL;
    }

    log_debug(kernel_log, "Atendiendo IO '%s' (fd=%d)", dispositivo_io->nombre, fd_io);

    op_code cop;
    while (recv(fd_io, &cop, sizeof(op_code), 0) > 0) {
        switch (cop) {
            case IO_FINALIZADA_OP: {
                log_debug(kernel_log, "IO_FINALIZADA_OP recibido de '%s' (fd=%d)", dispositivo_io->nombre, fd_io);
                
                // Recibir el PID del proceso que finalizó
                uint16_t pid_finalizado;
                if (recv(fd_io, &pid_finalizado, sizeof(uint16_t), 0) <= 0) {
                    log_error(kernel_log, "Error al recibir PID finalizado de IO '%s'", dispositivo_io->nombre);
                    continue;
                }
                
                log_info(kernel_log, "IO '%s' finalizó para PID=%d", dispositivo_io->nombre, pid_finalizado);
                
                // Procesar finalización de IO
                fin_io(dispositivo_io, pid_finalizado);
                break;
            }
                
            default:
                log_error(kernel_log, "Código op desconocido recibido desde IO '%s' (fd=%d): %d", 
                          dispositivo_io->nombre, fd_io, cop);
                break;
        }
    }

    // Si llegamos aquí, la IO se desconectó
    log_warning(kernel_log, "IO '%s' desconectada (fd=%d)", dispositivo_io->nombre, fd_io);
    
    // Manejar la desconexión: mover todos los procesos bloqueados por esta IO a EXIT
    pthread_mutex_lock(&mutex_ios);
    dispositivo_io->estado = IO_DISPONIBLE; // Para evitar que se envíen nuevos procesos
    
    // Crear lista con PCBs afectados por la desconexión
    t_list* pcbs_afectados = list_create();
    
    // Filtrar manualmente los PCBs bloqueados por esta IO
    for (int i = 0; i < list_size(pcbs_bloqueados_por_io); i++) {
        t_pcb_io* pcb_io = list_get(pcbs_bloqueados_por_io, i);
        if (pcb_io->io == dispositivo_io) {
            list_add(pcbs_afectados, pcb_io);
        }
    }
    
    // Remover estos PCBs de la lista de bloqueados
    t_pcb_io* pcb_io_actual = NULL;
    for (int i = 0; i < list_size(pcbs_afectados); i++) {
        pcb_io_actual = list_get(pcbs_afectados, i);
        t_pcb* pcb = pcb_io_actual->pcb;
        
        log_warning(kernel_log, "Proceso PID=%d en IO desconectada, moviendo a EXIT", pcb->PID);
        
        // Quitar de la lista de bloqueados manualmente
        for (int j = 0; j < list_size(pcbs_bloqueados_por_io); j++) {
            if (list_get(pcbs_bloqueados_por_io, j) == pcb_io_actual) {
                list_remove(pcbs_bloqueados_por_io, j);
                break;
            }
        }
        
        // Mover a EXIT
        cambiar_estado_pcb(pcb, EXIT_ESTADO);
        
        // Liberar memoria
        free(pcb_io_actual);
    }
    
    // Eliminar la IO de la lista
    io* io_eliminada = NULL;
    for (int i = 0; i < list_size(lista_ios); i++) {
        if (list_get(lista_ios, i) == dispositivo_io) {
            io_eliminada = list_remove(lista_ios, i);
            break;
        }
    }
    
    pthread_mutex_unlock(&mutex_ios);
    
    // Liberar recursos
    list_destroy(pcbs_afectados);
    if (io_eliminada) {
        free(io_eliminada->nombre);
        free(io_eliminada);
    }
    
    close(fd_io);
    return NULL;
}

// Implementaciones de funciones auxiliares
static bool es_bloqueado_por_io(void* elemento, io* dispositivo_io) {
    t_pcb_io* pcb_io = (t_pcb_io*) elemento;
    return pcb_io->io == dispositivo_io;
}

static bool es_misma_instancia(void* e, t_pcb_io* instancia) {
    return e == instancia;
}

static bool es_misma_io(void* elemento, io* disp_io) {
    io* io_ptr = (io*) elemento;
    return io_ptr == disp_io;
}
