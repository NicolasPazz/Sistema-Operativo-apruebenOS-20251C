#include "../headers/io.h"
#include <sys/time.h>
#include <stdio.h>

/////////////////////////////// Inicializacion de variables globales ///////////////////////////////

t_log *io_log;
t_config *io_config;
int fd_kernel_io;
char *IP_KERNEL;
char *PUERTO_KERNEL;
char *LOG_LEVEL;

void iniciar_config_io()
{
    const char *path_cfg = "io/io.config";
    io_config = iniciar_config((char *)path_cfg);

    IP_KERNEL = config_get_string_value(io_config, "IP_KERNEL");
    PUERTO_KERNEL = config_get_string_value(io_config, "PUERTO_KERNEL");
    LOG_LEVEL = config_get_string_value(io_config, "LOG_LEVEL");

    if (!(IP_KERNEL && PUERTO_KERNEL && LOG_LEVEL))
    {
        fprintf(stderr,
                "iniciar_config_io: Faltan campos obligatorios en %s\n",
                path_cfg);
        exit(EXIT_FAILURE);
    }

    printf("        Config leída: %s\n", path_cfg);
    printf("    IP_KERNEL      : %s\n", IP_KERNEL);
    printf("    PUERTO_KERNEL  : %s\n", PUERTO_KERNEL);
    printf("    LOG_LEVEL      : %s\n\n", LOG_LEVEL);
}

void iniciar_logger_io()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm *t = localtime(&tv.tv_sec);

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d%02d%02d_%02d%02d%02d_%03ld",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec,
             tv.tv_usec / 1000);

    char nombre_logger[256];
    snprintf(nombre_logger, sizeof(nombre_logger), "IO_%s_%s", nombre_io, timestamp);

    char path_logger[256];
    snprintf(path_logger, sizeof(path_logger), "io/io_%s_%s.log", nombre_io, timestamp);

    io_log = iniciar_logger(path_logger, nombre_logger, 1, log_level_from_string(LOG_LEVEL));
    if (io_log == NULL)
    {
        printf("Error al iniciar IO logs\n");
    }
    else
    {
        log_trace(io_log, "IO logs iniciados correctamente!");
    }
}

void iniciar_conexiones_io(char *nombre_io)
{
    log_trace(io_log, "Iniciando conexión con Kernel...");
    log_trace(io_log, "Intentando conectar a Kernel en %s:%s", IP_KERNEL, PUERTO_KERNEL);

    //////////////////////////// Conexion hacia Kernel ////////////////////////////

    fd_kernel_io = crear_conexion(IP_KERNEL, PUERTO_KERNEL, io_log);
    if (fd_kernel_io == -1)
    {
        log_error(io_log, "Error crítico: No se pudo conectar IO a Kernel en %s:%s", IP_KERNEL, PUERTO_KERNEL);
        exit(EXIT_FAILURE);
    }

    log_trace(io_log, "Socket creado exitosamente (fd=%d). Enviando handshake...", fd_kernel_io);

    int handshake = HANDSHAKE_IO_KERNEL;
    if (send(fd_kernel_io, &handshake, sizeof(int), 0) <= 0)
    {
        log_error(io_log, "Error al enviar handshake a Kernel: %s", strerror(errno));
        close(fd_kernel_io);
        exit(EXIT_FAILURE);
    }

    log_trace(io_log, "Handshake enviado. Enviando nombre del dispositivo: '%s'", nombre_io);

    if (send(fd_kernel_io, nombre_io, strlen(nombre_io) + 1, 0) <= 0)
    {
        log_error(io_log, "Error al enviar el nombre de IO a Kernel: %s", strerror(errno));
        close(fd_kernel_io);
        exit(EXIT_FAILURE);
    }

    log_trace(io_log, "✓ Dispositivo IO '%s' conectado exitosamente a Kernel (fd=%d)", nombre_io, fd_kernel_io);
    log_trace(io_log, "HANDSHAKE_IO_KERNEL completado correctamente");
}

void terminar_io()
{
    log_trace(io_log, "=== Iniciando terminación limpia del dispositivo IO ===");

    // Cerrar conexión con Kernel
    if (fd_kernel_io > 0)
    {
        log_trace(io_log, "Cerrando conexión con Kernel (fd=%d)...", fd_kernel_io);
        if (close(fd_kernel_io) == 0)
        {
            log_trace(io_log, "✓ Conexión con Kernel cerrada correctamente");
        }
        else
        {
            log_trace(io_log, "⚠ Error al cerrar conexión con Kernel: %s", strerror(errno));
        }
        fd_kernel_io = -1;
    }
    else
    {
        log_trace(io_log, "No hay conexión activa con Kernel para cerrar");
    }

    /*// Liberar recursos de configuración
    if (io_config != NULL) {
        config_destroy(io_config);
        io_config = NULL;
        log_trace(io_log, "✓ Configuración IO liberada correctamente");
    }

    // Finalizar logging
    if (io_log != NULL) {
        log_info(io_log, "✓ Dispositivo IO terminado correctamente");
        log_destroy(io_log);
        io_log = NULL;
    }*/

    log_info(io_log, "IO finalizado correctamente.");
}

double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}