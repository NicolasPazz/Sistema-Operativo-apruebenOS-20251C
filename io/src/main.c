#include "../headers/io.h"
#include <signal.h>

// Manejador de señales para terminación limpia
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n\nRecibida señal de terminación. Cerrando IO...\n");
        log_trace(io_log, "Recibida señal SIGINT. Iniciando terminación limpia de IO...");
        terminar_io();
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char* argv[]) {
    // Configurar el manejador de señales
    signal(SIGINT, signal_handler);
    
    if (argc < 2) {
        fprintf(stderr, "[IO] Uso: %s <NOMBRE_IO>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* nombre_io = argv[1];

    iniciar_config_io();
    iniciar_logger_io();

    log_trace(io_log, "=== Iniciando dispositivo IO: %s ===", nombre_io);
    log_trace(io_log, "Configuración cargada - IP_KERNEL: %s, PUERTO_KERNEL: %s", IP_KERNEL, PUERTO_KERNEL);

    iniciar_conexiones_io(nombre_io);
    
    log_trace(io_log, "Dispositivo IO %s listo para recibir operaciones del Kernel", nombre_io);
    
    // Bucle principal para atender operaciones del Kernel
    while (1) {

        int cop = recibir_operacion(fd_kernel_io);

        log_trace(io_log, "Operación recibida del Kernel: %d", cop);
    
        switch (cop) {
            case IO_OP: {
                log_trace(io_log, "Procesando operación IO_OP...");
            
                int buffer[2];
                if (!recibir_enteros(fd_kernel_io, buffer, 2)) {
                    log_error(io_log, "Error al recibir datos de IO_OP");
                    break;
                }
                int pid = buffer[0];
                int tiempo_io = buffer[1];
            
                log_debug(io_log, "PID recibido: %d | Tiempo de IO: %d", pid, tiempo_io);
            
                log_info(io_log, "## PID: %d - Inicio de IO - Tiempo: %d", pid, tiempo_io);
                log_trace(io_log, "Simulando operación de I/O para PID %d durante %.3f milisegundos...", pid, (double)tiempo_io/1000);
                usleep(tiempo_io);
                log_info(io_log, "## PID: %d - Fin de IO", pid);
            
                op_code finalizado = IO_FINALIZADA_OP;
                if (send(fd_kernel_io, &finalizado, sizeof(op_code), 0) <= 0 ||
                    send(fd_kernel_io, &pid, sizeof(int), 0) <= 0) {
                    log_error(io_log, "Error al notificar finalización de IO al Kernel: %s", strerror(errno));
                    break;
                }
            
                log_trace(io_log, "Notificación de finalización enviada al Kernel para PID %d", pid);
                break;
            }
            

            default:
                log_warning(io_log, "Se recibió un op_code inesperado: %d", cop);
                terminar_io();
                exit(EXIT_FAILURE);
                break;
        }
    }
    
    log_trace(io_log, "Finalizando dispositivo IO %s...", nombre_io);
    terminar_io();
    return EXIT_SUCCESS;
}

void iterator(char* value) {
    log_trace(io_log, "%s", value);
}
