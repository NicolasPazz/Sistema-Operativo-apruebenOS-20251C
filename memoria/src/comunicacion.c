#include "../headers/comunicacion.h"

/////////////////////////////// Inicialización de variables globales ///////////////////////////////


int fd_memoria;
int fd_kernel;
int fd_cpu;
extern t_log* logger;

typedef struct {
    int fd;
    char* server_name;
} t_procesar_conexion_args;


int iniciar_conexiones_memoria(char* PUERTO_ESCUCHA, t_log* logger) {
    if (logger == NULL) {
        printf("Error: Logger no inicializado\n");
        exit(EXIT_FAILURE);
    }

    fd_memoria = iniciar_servidor(PUERTO_ESCUCHA, logger, "Server Memoria iniciado");

    if (fd_memoria == -1) {
        log_error(logger, "No se pudo iniciar el servidor de Memoria");
        exit(EXIT_FAILURE);
    }

    log_info(logger, "Esperando conexiones entrantes en Memoria...");
    return fd_memoria; // Devuelve el socket del servidor
}

int server_escuchar(char* server_name, int server_socket) {
    if (logger == NULL) {
        printf("Error: Logger no inicializado en server_escuchar\n");
        return 0;
    }
    int cliente_socket = esperar_cliente(server_socket, logger);

    if (cliente_socket != -1) {
        pthread_t hilo;
        t_procesar_conexion_args* args = malloc(sizeof(t_procesar_conexion_args));
        args->fd = cliente_socket;
        args->server_name = server_name;
        pthread_create(&hilo, NULL, (void*) procesar_conexion, (void*) args);
        pthread_detach(hilo);
        return 1;
    }
    return 0;
}

void procesar_conexion(void* void_args) {
    t_procesar_conexion_args* args = (t_procesar_conexion_args*) void_args;
    if (args == NULL) {
        log_error(logger, "Error: Argumentos de conexión nulos");
        return;
    }

    int cliente_socket = args->fd;
    char* server_name = args->server_name;
    free(args);

    int handshake = -1;

    if (recv(cliente_socket, &handshake, sizeof(int), 0) <= 0) {
        log_error(logger, "Error al recibir handshake del cliente (fd=%d): %s", cliente_socket, strerror(errno));
        close(cliente_socket);
        return;
    }
    switch (handshake) {
        case HANDSHAKE_MEMORIA_KERNEL:
            log_info(logger, "HANDSHAKE_MEMORIA_KERNEL: Se conectó el Kernel (fd=%d)", cliente_socket);
            fd_kernel = cliente_socket;
            break;

        case HANDSHAKE_MEMORIA_CPU:
            log_info(logger, "HANDSHAKE_MEMORIA_CPU: Se conectó una CPU (fd=%d)", cliente_socket);
            fd_cpu = cliente_socket;
            break;

        default:
            log_error(logger, "Handshake inválido recibido (fd=%d): %d", cliente_socket, handshake);
            close(cliente_socket);
            return;
    }

    log_info(logger, "Conexión procesada exitosamente para %s (fd=%d)", server_name, cliente_socket);
}
