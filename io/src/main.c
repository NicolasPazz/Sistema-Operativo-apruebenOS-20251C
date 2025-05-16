#include "../headers/io.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "[IO] Uso: %s <NOMBRE_IO>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* nombre_io = argv[1];

    iniciar_config_io();
    iniciar_logger_io();

    log_debug(io_log, "Iniciando proceso IO: %s", nombre_io);

    iniciar_conexiones_io(nombre_io);
    while (1) {
        op_code cop;
        if (recv(fd_kernel_io, &cop, sizeof(op_code), 0) <= 0) {
            log_error(io_log, "Error al recibir op_code del Kernel: %s", strerror(errno));
            break;
        }
    
        switch (cop) {
            case IO_OP: {
                // Recibir y deserializar el PID y tiempo_io
                uint16_t pid;
                uint16_t tiempo_io;
    
                if (recv(fd_kernel_io, &pid, sizeof(uint16_t), 0) <= 0) {
                    log_error(io_log, "Error al recibir PID desde el Kernel: %s", strerror(errno));
                    break;
                }
    
                if (recv(fd_kernel_io, &tiempo_io, sizeof(uint16_t), 0) <= 0) {
                    log_error(io_log, "Error al recibir tiempo de IO desde el Kernel: %s", strerror(errno));
                    break;
                }
    
                t_pedido_io pedido;
                pedido.pid = pid;
                pedido.tiempo_io = tiempo_io;
    
                log_info(io_log, "## PID: %d - Inicio de IO - Tiempo: %ld", pedido.pid, pedido.tiempo_io);
                usleep(pedido.tiempo_io * 1000); // milisegundos a microsegundos
                log_info(io_log, "## PID: %d - Fin de IO", pedido.pid);
    
                op_code finalizado = IO_FINALIZADA_OP;
                if (send(fd_kernel_io, &finalizado, sizeof(op_code), 0) <= 0) {
                    log_error(io_log, "Error al enviar IO_FINALIZADA_OP al Kernel: %s", strerror(errno));
                    break;
                }
    
                uint16_t pid_finalizado = (uint16_t) pedido.pid;
                if (send(fd_kernel_io, &pid_finalizado, sizeof(uint16_t), 0) <= 0) {
                    log_error(io_log, "Error al enviar PID finalizado al Kernel: %s", strerror(errno));
                    break;
                }
    
                break;
            }
            default:
                log_warning(io_log, "Se recibió un op_code inesperado: %d", cop);
                break;
        }
    }
    return EXIT_SUCCESS;
}

// void atender_cliente(void* arg) {
//     cliente_data_t *data = (cliente_data_t *)arg;
//     int control_key = 1;
//     while (control_key) {
//         int cod_op = recibir_operacion(data->fd);
//         switch (cod_op) {
//             case MENSAJE_OP:
//                 recibir_mensaje(data->fd, data->logger);
//                 break;
//             case PAQUETE_OP:
//                 t_list* lista = recibir_paquete(data->fd);
//                 list_iterate(lista, (void*)iterator);
//                 list_destroy(lista);
//                 break;
//             case -1:
//                 log_error(data->logger, "El cliente (%s) se desconecto. Terminando servidor.", data->cliente);
//                 control_key = 0;
//                 break;
//             default:
//                 log_error(data->logger, "Operacion desconocida de %s", data->cliente);
//                 break;
//         }
//     }
// }

void iterator(char* value) {
    log_debug(io_log, "%s", value);
}
