#include "../headers/init.h"

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

void leer_config_cpu() {
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

void conectar_cpu_memoria() {

    if((fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA)) == -1){
        log_trace(cpu_log, "Error al conectar con Memoria. El servidor no esta activo");
        log_info(cpu_log, "Error al conectar CPU a Memoria");
        exit(EXIT_FAILURE);
    } else {
        log_info(cpu_log, "CPU conectado a Memoria exitosamente");
    }
}

void conectar_kernel_dispatch() {
    fd_kernel_dispatch = crear_conexion(IP_KERNEL, PUERTO_KERNEL_DISPATCH);
    if (fd_kernel_dispatch != -1) {
        log_info(cpu_log, "CPU conectado a Kernel Dispatch exitosamente");
    } else {
        log_error(cpu_log, "Error al conectar CPU a Kernel Dispatch");
        exit(EXIT_FAILURE);
    }
}

void conectar_kernel_interrupt() {
    fd_kernel_interrupt = crear_conexion(IP_KERNEL, PUERTO_KERNEL_INTERRUPT);
    if (fd_kernel_interrupt != -1) {
        log_info(cpu_log, "CPU conectado a Kernel Interrupt exitosamente");
    } else {
        log_error(cpu_log, "Error al conectar CPU a Kernel Interrupt");
        exit(EXIT_FAILURE);
    }
}


int realizar_handshake(char* ip, char* puerto, char* quien_realiza_solicitud, char* con_quien_se_conecta) {
    // Crear conexión al servidor
    int conexion = crear_conexion(ip, puerto);

    if (conexion == -1) {
        perror("Error al crear conexión");
        log_error(cpu_log, "No se pudo conectar a %s:%s", ip, puerto);
        return -1;
    }

    uint32_t handshake = 1; // Código del handshake
    uint32_t result;        // Respuesta del servidor
    int bytes_enviados, bytes_recibidos;

    // Enviar el handshake
    bytes_enviados = send(conexion, &handshake, sizeof(handshake), MSG_NOSIGNAL);
    if (bytes_enviados == -1) {
        perror("Error al enviar handshake");
        log_error(cpu_log, "Fallo al enviar handshake a %s:%s", ip, puerto);
        close(conexion);
        return -1;
    }

    // Esperar la respuesta del servidor
    bytes_recibidos = recv(conexion, &result, sizeof(result), MSG_WAITALL);
    if (bytes_recibidos <= 0) {
        if (bytes_recibidos == 0) {
            log_error(cpu_log, "El servidor %s:%s cerró la conexión inesperadamente", ip, puerto);
        } else {
            perror("Error al recibir respuesta");
        }
        close(conexion);
        return -1;
    }

    // Validar la respuesta del handshake
    if (result != 0) {
        log_error(cpu_log, "Handshake rechazado por el servidor %s:%s", ip, puerto);
        close(conexion);
        return -1;
    }

    // Loggear éxito del handshake
    log_info(cpu_log, "Handshake exitoso. %s se conectó con %s en %s:%s",
             quien_realiza_solicitud, con_quien_se_conecta, ip, puerto);

    // Devolver el descriptor de la conexión
    return conexion;
}
