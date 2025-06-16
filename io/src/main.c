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
    bool continuar_ejecucion = true;
    while (continuar_ejecucion) {
        op_code cop;
        int bytes_recibidos = recv(fd_kernel_io, &cop, sizeof(op_code), 0);
        
        // Verificar si el Kernel se desconectó
        if (bytes_recibidos <= 0) {
            if (bytes_recibidos == 0) {
                log_warning(io_log, "El Kernel se desconectó correctamente. Finalizando dispositivo IO %s...", nombre_io);
            } else {
                log_error(io_log, "Error al recibir op_code del Kernel: %s. Finalizando dispositivo IO %s...", 
                         strerror(errno), nombre_io);
            }
            continuar_ejecucion = false;
            break;
        }
        
        log_trace(io_log, "Operación recibida del Kernel: %d", cop);
    
        switch (cop) {
            case IO_OP: {
                log_trace(io_log, "Procesando operación IO_OP...");
                
                // Recibir y deserializar el PID y tiempo_io
                int pid;
                int tiempo_io;
    
                if (recv(fd_kernel_io, &pid, sizeof(int), 0) <= 0) {
                    log_error(io_log, "Error al recibir PID desde el Kernel: %s", strerror(errno));
                    continuar_ejecucion = false;
                    break;
                }
    
                if (recv(fd_kernel_io, &tiempo_io, sizeof(int), 0) <= 0) {
                    log_error(io_log, "Error al recibir tiempo de IO desde el Kernel: %s", strerror(errno));
                    continuar_ejecucion = false;
                    break;
                }
    
                t_pedido_io pedido;
                pedido.pid = pid;
                pedido.tiempo_io = tiempo_io;
    
                // Log obligatorio según consigna: Inicio de IO
                log_info(io_log, "## PID: %d - Inicio de IO - Tiempo: %ld", pedido.pid, pedido.tiempo_io);
                
                // Simular la operación de I/O
                log_trace(io_log, "Simulando operación de I/O para PID %d durante %ld unidades de tiempo...", 
                         pedido.pid, pedido.tiempo_io);
                sleep(pedido.tiempo_io);
                
                // Log obligatorio según consigna: Finalización de IO
                log_info(io_log, "## PID: %d - Fin de IO", pedido.pid);
    
                // Notificar al Kernel que la operación finalizó
                op_code finalizado = IO_FINALIZADA_OP;
                if (send(fd_kernel_io, &finalizado, sizeof(op_code), 0) <= 0) {
                    log_error(io_log, "Error al enviar IO_FINALIZADA_OP al Kernel: %s", strerror(errno));
                    continuar_ejecucion = false;
                    break;
                }
    
                int pid_finalizado = (int) pedido.pid;
                if (send(fd_kernel_io, &pid_finalizado, sizeof(int), 0) <= 0) {
                    log_error(io_log, "Error al enviar PID finalizado al Kernel: %s", strerror(errno));
                    continuar_ejecucion = false;
                    break;
                }
                
                log_trace(io_log, "Notificación de finalización enviada al Kernel para PID %d", pedido.pid);
                break;
            }
            default:
                log_warning(io_log, "Se recibió un op_code inesperado: %d. Ignorando operación...", cop);
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
