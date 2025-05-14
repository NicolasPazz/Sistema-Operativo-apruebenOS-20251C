#include "../headers/io.h"
/////////////////////////////// Inicializacion de variables globales ///////////////////////////////
t_log* io_log;
t_config* io_config;
int fd_kernel_io;
char* IP_KERNEL;
char* PUERTO_KERNEL;
char* LOG_LEVEL;

void iniciar_config_io() {
    io_config = iniciar_config("io.config");

    IP_KERNEL = config_get_string_value(io_config, "IP_KERNEL");
    PUERTO_KERNEL= config_get_string_value(io_config, "PUERTO_KERNEL");
    LOG_LEVEL = config_get_string_value(io_config, "LOG_LEVEL");

    if (IP_KERNEL && PUERTO_KERNEL && LOG_LEVEL) {
    } else {
        printf("Error al leer io.config\n");
    }
}

void iniciar_logger_io() {
    io_log = iniciar_logger("io.log", "io", 1, log_level_from_string(LOG_LEVEL));
    if (io_log == NULL) {
        printf("Error al iniciar IO logs\n");
    } else {
        log_debug(io_log, "IO logs iniciados correctamente!");
    }
}

void iniciar_conexiones_io(char* nombre_io){
    //////////////////////////// Conexion hacia Kernel ////////////////////////////
    fd_kernel_io = crear_conexion(IP_KERNEL, PUERTO_KERNEL, io_log);
    if (fd_kernel_io == -1) {
        log_error(io_log, "Error al conectar IO a Kernel");
        exit(EXIT_FAILURE);
    }

    int handshake = HANDSHAKE_IO_KERNEL;
    if (send(fd_kernel_io, &handshake, sizeof(int), 0) <= 0) {
        log_error(io_log, "Error al enviar handshake a Kernel: %s", strerror(errno));
        close(fd_kernel_io);
        exit(EXIT_FAILURE);
    }

    if (send(fd_kernel_io, nombre_io, strlen(nombre_io) + 1, 0) <= 0) {
        log_error(io_log, "Error al enviar el nombre de IO a Kernel: %s", strerror(errno));
        close(fd_kernel_io);
        exit(EXIT_FAILURE);
    }

    log_debug(io_log, "HANDSHAKE_IO_KERNEL: IO conectado exitosamente a Kernel (fd=%d)", fd_kernel_io);
}