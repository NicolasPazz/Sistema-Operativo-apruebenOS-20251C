#include "../headers/io.h"
#include <signal.h>

char* nombre_io;
// Manejador de señales para terminación limpia
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nRecibida señal de terminación. Cerrando IO...\n");
        log_info(io_log, "Recibida señal de terminación. Cerrando IO...");
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

    nombre_io = argv[1];

    iniciar_config_io();
    iniciar_logger_io();

    log_trace(io_log, AZUL("=== Iniciando dispositivo IO: %s ==="), nombre_io);
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

                // ========== RECIBIR PARÁMETROS DESDE KERNEL ==========
                t_list* parametros_io = recibir_contenido_paquete(fd_kernel_io);
                if (!parametros_io || list_size(parametros_io) < 3) {
                    log_error(io_log, "Error al recibir paquete de IO_OP");
                    if (parametros_io) list_destroy_and_destroy_elements(parametros_io, free);
                    break;
                }

                // nombre_io, tiempo_io y pid
                char* nombre_io = (char*)list_get(parametros_io, 0);
                int tiempo_io = *(int*)list_get(parametros_io, 1);
                int pid = *(int*)list_get(parametros_io, 2);

                log_debug(io_log, "PID recibido: %d | Tiempo de IO: %d | Dispositivo: %s", pid, tiempo_io, nombre_io);

                log_info(io_log, VERDE("## PID: %d - Inicio de IO - Tiempo: %d"), pid, tiempo_io);
                log_trace(io_log, "Simulando operación de I/O para PID %d durante %.d milisegundos...", pid, tiempo_io);

                double inicio = get_time();

                int resultado = usleep(tiempo_io * 1000); // usleep usa microsegundos: 1 ms = 1000 µs
                if(resultado != 0) {
                    log_error(io_log, "Error al simular IO para PID %d: %s", pid, strerror(errno));
                    list_destroy_and_destroy_elements(parametros_io, free);
                    terminar_io();
                    exit(EXIT_FAILURE);
                }
                log_info(io_log, VERDE("## PID: %d - Fin de IO"), pid);
                log_debug(io_log, "Operación de I/O para PID %d finalizada en %.3f milisegundos", pid, get_time() - inicio);
            
                op_code finalizado = IO_FINALIZADA_OP;
                if (send(fd_kernel_io, &finalizado, sizeof(op_code), 0) <= 0 ||
                    send(fd_kernel_io, &pid, sizeof(int), 0) <= 0) {
                    log_error(io_log, "Error al notificar finalización de IO al Kernel: %s", strerror(errno));
                    list_destroy_and_destroy_elements(parametros_io, free);
                    break;
                }
            
                log_trace(io_log, "Notificación de finalización enviada al Kernel para PID %d", pid);
                list_destroy_and_destroy_elements(parametros_io, free);
                break;
            }
            case -1:
                log_debug(io_log, "Se desconectó el Kernel. Finalizando IO...");
                terminar_io();
                exit(EXIT_SUCCESS);
            default:
                log_error(io_log, "Operación desconocida recibida del Kernel: %d", cop);
                terminar_io();
                exit(EXIT_FAILURE);
        }
    }
    
    log_trace(io_log, "Finalizando dispositivo IO %s...", nombre_io);
    terminar_io();
    return EXIT_SUCCESS;
}

void iterator(char* value) {
    log_trace(io_log, "%s", value);
}
