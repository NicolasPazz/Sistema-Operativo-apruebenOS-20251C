#include "../headers/kernel.h"

/////////////////////////////// Inicialización de variables globales ///////////////////////////////
t_log* kernel_log;

int fd_dispatch;
int fd_interrupt;
int fd_memoria;

t_config* kernel_config;
char* IP_MEMORIA;
char* PUERTO_MEMORIA;
char* PUERTO_ESCUCHA_DISPATCH;
char* PUERTO_ESCUCHA_INTERRUPT;
char* PUERTO_ESCUCHA_IO;
char* ALGORITMO_PLANIFICACION;
char* ALGORITMO_COLA_NEW;
char* ALFA;
char* TIEMPO_SUSPENSION;
char* LOG_LEVEL;

void iniciar_config_kernel() {
    kernel_config = iniciar_config("kernel.config");

    IP_MEMORIA = config_get_string_value(kernel_config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_string_value(kernel_config, "PUERTO_MEMORIA");
    PUERTO_ESCUCHA_DISPATCH = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_DISPATCH");
    PUERTO_ESCUCHA_INTERRUPT = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_INTERRUPT");
    PUERTO_ESCUCHA_IO = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_IO");
    ALGORITMO_PLANIFICACION = config_get_string_value(kernel_config, "ALGORITMO_PLANIFICACION");
    ALGORITMO_COLA_NEW = config_get_string_value(kernel_config, "ALGORITMO_COLA_NEW");
    ALFA = config_get_string_value(kernel_config, "ALFA");
    TIEMPO_SUSPENSION = config_get_string_value(kernel_config, "TIEMPO_SUSPENSION");
    LOG_LEVEL = config_get_string_value(kernel_config, "LOG_LEVEL");

    if (IP_MEMORIA && PUERTO_MEMORIA &&
        PUERTO_ESCUCHA_DISPATCH && PUERTO_ESCUCHA_INTERRUPT &&
        PUERTO_ESCUCHA_IO && ALGORITMO_PLANIFICACION &&
        ALGORITMO_COLA_NEW && ALFA && TIEMPO_SUSPENSION &&
        LOG_LEVEL) {
        //printf("Kernel config leído correctamente\n");
    } else {
        printf("Error al leer kernel config\n");
    }
}

void iniciar_logger_kernel() {
    kernel_log = iniciar_logger("kernel.log", "kernel", 1, log_level_from_string(LOG_LEVEL));
    if (kernel_log == NULL) {
        printf("Error al iniciar kernel logs\n");
    } else {
        log_info(kernel_log, "Kernel logs iniciados correctamente!");
    }
}

void iniciar_conexiones_kernel(){
    //////////////////////////// Conexión hacia Memoria ////////////////////////////
    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA);
    if (fd_memoria != -1) {
        log_info(kernel_log, "Kernel conectado a Memoria exitosamente");
    } else {
        log_info(kernel_log, "Error al conectar Kernel a Memoria");
        exit(EXIT_FAILURE);
    }

    //////////////////////////// Iniciar Servidor Dispatch ////////////////////////////
    fd_dispatch= iniciar_servidor(PUERTO_ESCUCHA_DISPATCH, kernel_log, "Escuchando conexiones en Dispatch...");
    if (fd_dispatch != -1) {
    } else {
        log_info(kernel_log, "Error al iniciar servidor Dispatch del Kernel");
        exit(EXIT_FAILURE);
    }

    //////////////////////////// Iniciar Servidor Interrupt ////////////////////////////
    fd_interrupt= iniciar_servidor(PUERTO_ESCUCHA_INTERRUPT, kernel_log, "Escuchando conexiones en Interrupt...");
    if (fd_interrupt != -1) {
    } else {
        log_info(kernel_log, "Error al iniciar servidor Interrupt del Kernel");
        exit(EXIT_FAILURE);
    }


}