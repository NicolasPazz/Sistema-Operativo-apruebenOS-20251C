#include "../headers/kernel.h"

/////////////////////////////// Declaración de variables globales ///////////////////////////////
// Logger
t_log* kernel_log;
t_log* kernel_log_debug;

// Sockets
int fd_dispatch;
int fd_interrupt;
int fd_memoria;

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
    TIEMPO_SUSPENSION = config_get_string_value(kernel_config, "TIEMPO_SUSPENSION");
    LOG_LEVEL = config_get_string_value(kernel_config, "LOG_LEVEL");

    if (!IP_MEMORIA || !PUERTO_MEMORIA || !PUERTO_ESCUCHA_DISPATCH || !PUERTO_ESCUCHA_INTERRUPT ||
        !PUERTO_ESCUCHA_IO || !ALGORITMO_CORTO_PLAZO || !ALGORITMO_INGRESO_A_READY ||
        !ALFA || !TIEMPO_SUSPENSION || !LOG_LEVEL) {
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

void iniciar_conexiones_kernel(){
    //////////////////////////// Conexión hacia Memoria ////////////////////////////
    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA);
    if (fd_memoria != -1) {
        log_info(kernel_log, "Kernel conectado a Memoria exitosamente");
    } else {
        log_error(kernel_log, "iniciar_conexiones_kernel: Error al conectar Kernel a Memoria");
        exit(EXIT_FAILURE);
    }
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