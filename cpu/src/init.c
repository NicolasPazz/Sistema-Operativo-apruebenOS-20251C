#include "../headers/cpu.h"

/////////////////////////////// Inicialización de variables globales ///////////////////////////////
t_log* cpu_log;
t_config* cpu_config;

int fd_memoria;
int fd_kernel_dispatch;
int fd_kernel_interrupt;

char* IP_MEMORIA;
char* PUERTO_MEMORIA;
char* IP_KERNEL;
char* PUERTO_KERNEL_DISPATCH;
char* PUERTO_KERNEL_INTERRUPT;
char* ENTRADAS_TLB;
char* REEMPLAZO_TLB;
char* ENTRADAS_CACHE;
char* REEMPLAZO_CACHE;
char* RETARDO_CACHE;
char* LOG_LEVEL;

void iniciar_config_cpu() {
    cpu_config = iniciar_config("cpu.config");

    IP_MEMORIA = config_get_string_value(cpu_config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_string_value(cpu_config, "PUERTO_MEMORIA");
    IP_KERNEL = config_get_string_value(cpu_config, "IP_KERNEL");
    PUERTO_KERNEL_DISPATCH = config_get_string_value(cpu_config, "PUERTO_KERNEL_DISPATCH");
    PUERTO_KERNEL_INTERRUPT = config_get_string_value(cpu_config, "PUERTO_KERNEL_INTERRUPT");
    ENTRADAS_TLB = config_get_string_value(cpu_config, "ENTRADAS_TLB");
    REEMPLAZO_TLB = config_get_string_value(cpu_config, "REEMPLAZO_TLB");
    ENTRADAS_CACHE = config_get_string_value(cpu_config, "ENTRADAS_CACHE");
    REEMPLAZO_CACHE = config_get_string_value(cpu_config, "REEMPLAZO_CACHE");
    RETARDO_CACHE = config_get_string_value(cpu_config, "RETARDO_CACHE");
    LOG_LEVEL = config_get_string_value(cpu_config, "LOG_LEVEL");

    if (IP_MEMORIA && PUERTO_MEMORIA &&
        IP_KERNEL && PUERTO_KERNEL_DISPATCH && PUERTO_KERNEL_INTERRUPT &&
        ENTRADAS_TLB && REEMPLAZO_TLB &&
        ENTRADAS_CACHE && REEMPLAZO_CACHE &&
        RETARDO_CACHE && LOG_LEVEL) {
    } else {
        printf("Error al leer cpu.config\n");
    }
}

void iniciar_logger_cpu() {
    cpu_log = iniciar_logger("cpu.log", "cpu", 1, log_level_from_string(LOG_LEVEL));
    if (cpu_log == NULL) {
        printf("Error al iniciar cpu logs\n");
    } else {
        log_info(cpu_log, "CPU logs iniciados correctamente!");
    }
}

void iniciar_conexiones_cpu(){
    //////////////////////////// Conexión hacia Kernel Dispatch ////////////////////////////
    fd_kernel_dispatch = crear_conexion(IP_KERNEL, PUERTO_KERNEL_DISPATCH);
    if (fd_kernel_dispatch != -1) {
        log_info(cpu_log, "CPU conectado a Kernel Dispatch exitosamente");
    } else {
        log_info(cpu_log, "Error al conectar CPU a Kernel Dispatch");
        exit(EXIT_FAILURE);
    }

    //////////////////////////// Conexión hacia Kernel Interrupt ////////////////////////////
    fd_kernel_interrupt = crear_conexion(IP_KERNEL, PUERTO_KERNEL_INTERRUPT);
    if (fd_kernel_interrupt != -1) {
        log_info(cpu_log, "CPU conectado a Kernel Interrupt exitosamente");
    } else {
        log_info(cpu_log, "Error al conectar CPU a Kernel Interrupt");
        exit(EXIT_FAILURE);
    }

    //////////////////////////// Conexión hacia Memoria ////////////////////////////
    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA);
    if (fd_memoria != -1) {
        log_info(cpu_log, "CPU conectado a Memoria exitosamente");
    } else {
        log_info(cpu_log, "Error al conectar CPU a Memoria");
        exit(EXIT_FAILURE);
    }
}