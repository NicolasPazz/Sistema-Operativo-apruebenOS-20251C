#include "../headers/kernel.h"

/////////////////////////////// Declaración de variables globales ///////////////////////////////
// Logger
t_log* kernel_log;
t_log* kernel_log_debug;

// Hashmap de cronometros por PCB
t_dictionary* tiempos_por_pid;

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

// Listas y semaforos de CPUs y IOs conectadas
t_list* lista_cpus;
pthread_mutex_t mutex_lista_cpus;
t_list* lista_ios;
pthread_mutex_t mutex_ios;

// Conexiones minimas
bool conectado_cpu = false;
bool conectado_io = false;
pthread_mutex_t mutex_conexiones;

// Semaforos y condiciones de planificacion
pthread_mutex_t mutex_cola_new;
pthread_cond_t cond_nuevo_proceso;
pthread_cond_t cond_susp_ready_vacia;
pthread_mutex_t mutex_cola_susp_ready;
pthread_mutex_t mutex_cola_susp_blocked;
pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_running;
pthread_mutex_t mutex_cola_blocked;
pthread_mutex_t mutex_cola_exit;
pthread_cond_t cond_exit;
pthread_mutex_t mutex_replanificar_pmcp;
pthread_cond_t cond_replanificar_pmcp;

/////////////////////////////// Inicialización de variables globales ///////////////////////////////
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
        log_info(kernel_log_debug, "IP_MEMORIA: %s", IP_MEMORIA);
        log_info(kernel_log_debug, "PUERTO_MEMORIA: %s", PUERTO_MEMORIA);
        log_info(kernel_log_debug, "PUERTO_ESCUCHA_DISPATCH: %s", PUERTO_ESCUCHA_DISPATCH);
        log_info(kernel_log_debug, "PUERTO_ESCUCHA_INTERRUPT: %s", PUERTO_ESCUCHA_INTERRUPT);
        log_info(kernel_log_debug, "PUERTO_ESCUCHA_IO: %s", PUERTO_ESCUCHA_IO);
        log_info(kernel_log_debug, "ALGORITMO_CORTO_PLAZO: %s", ALGORITMO_CORTO_PLAZO);
        log_info(kernel_log_debug, "ALGORITMO_INGRESO_A_READY: %s", ALGORITMO_INGRESO_A_READY);
        log_info(kernel_log_debug, "ALFA: %s", ALFA);
        log_info(kernel_log_debug, "ESTIMACION_INICIAL: %s", ESTIMACION_INICIAL);
        log_info(kernel_log_debug, "TIEMPO_SUSPENSION: %s", TIEMPO_SUSPENSION);
        log_info(kernel_log_debug, "LOG_LEVEL: %s", LOG_LEVEL);
    }
}

void iniciar_logger_kernel() {
    kernel_log = iniciar_logger("kernel.log", "kernel", 1, log_level_from_string(LOG_LEVEL));
    log_info(kernel_log, "Kernel log iniciado correctamente!");
}

void iniciar_logger_kernel_debug() {
    kernel_log_debug = iniciar_logger("kernel_config_debug.log", "kernel", 1, LOG_LEVEL_TRACE);
    log_info(kernel_log_debug, "Kernel log de debug iniciado correctamente!");
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
}

void iniciar_sincronizacion_kernel() {
    pthread_mutex_init(&mutex_lista_cpus, NULL);
    pthread_mutex_init(&mutex_ios, NULL);
    pthread_mutex_init(&mutex_conexiones, NULL);

    pthread_mutex_init(&mutex_cola_new, NULL);
    pthread_cond_init(&cond_nuevo_proceso, NULL);
    pthread_cond_init(&cond_susp_ready_vacia, NULL);
    pthread_mutex_init(&mutex_cola_susp_ready, NULL);
    pthread_mutex_init(&mutex_cola_susp_blocked, NULL);
    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_running, NULL);
    pthread_mutex_init(&mutex_cola_blocked, NULL);
    pthread_mutex_init(&mutex_cola_exit, NULL);
    pthread_cond_init(&cond_exit, NULL);
    pthread_mutex_init(&mutex_replanificar_pmcp, NULL);
    pthread_cond_init(&cond_replanificar_pmcp, NULL);

    lista_cpus = list_create();
    lista_ios = list_create();

    conectado_cpu = false;
    conectado_io = false;
}


void iniciar_diccionario_tiempos() {
    tiempos_por_pid = dictionary_create();
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

    pthread_mutex_destroy(&mutex_lista_cpus);
    pthread_mutex_destroy(&mutex_ios);
    pthread_mutex_destroy(&mutex_conexiones);
    pthread_mutex_destroy(&mutex_cola_new);
    pthread_cond_destroy(&cond_nuevo_proceso);
    pthread_cond_destroy(&cond_susp_ready_vacia);
    pthread_mutex_destroy(&mutex_cola_susp_ready);
    pthread_mutex_destroy(&mutex_cola_susp_blocked);
    pthread_mutex_destroy(&mutex_cola_ready);
    pthread_mutex_destroy(&mutex_cola_running);
    pthread_mutex_destroy(&mutex_cola_blocked);
    pthread_mutex_destroy(&mutex_cola_exit);
    pthread_cond_destroy(&cond_exit);
    pthread_mutex_destroy(&mutex_replanificar_pmcp);
    pthread_cond_destroy(&cond_replanificar_pmcp);
    
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
    ////////// Conexión hacia Memoria //////////
    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, kernel_log);

    if (fd_memoria == -1) {
        log_error(kernel_log, "iniciar_conexiones_kernel: Error al conectar Kernel a Memoria");
        exit(EXIT_FAILURE);
    }

    int handshake = HANDSHAKE_MEMORIA_KERNEL;
    if (send(fd_memoria, &handshake, sizeof(int), 0) <= 0) {
        log_error(kernel_log, "Error al enviar handshake a Memoria");
        close(fd_memoria);
        exit(EXIT_FAILURE);
    }

    log_info(kernel_log, "HANDSHAKE_MEMORIA_KERNEL: Kernel conectado correctamente a Memoria (fd=%d)", fd_memoria);

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

        log_info(kernel_log, "HANDSHAKE_CPU_KERNEL_DISPATCH: CPU conectada exitosamente a Dispatch (fd=%d), ID=%d", nueva_cpu->fd, nueva_cpu->id);
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

        log_info(kernel_log, "HANDSHAKE_CPU_KERNEL_INTERRUPT: CPU conectada exitosamente a Interrupt (fd=%d), ID=%d", nueva_cpu->fd, nueva_cpu->id);
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

        log_info(kernel_log, "HANDSHAKE_IO_KERNEL: IO '%s' conectada exitosamente (fd=%d)", nueva_io->nombre, fd_io);
    }

    return NULL;
}

