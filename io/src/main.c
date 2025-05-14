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
    while(1){
        usleep(10000); // lo dejamos para no consumir CPU innecesariamente cuando no hay actividad
        t_pedido_io pedido;
        ssize_t bytes_recibidos = recv(fd_kernel_io, &pedido, sizeof(t_pedido_io), 0);

        if (bytes_recibidos > 0) {
            log_info(io_log, "## PID: %d - Inicio de IO - Tiempo: %ld", pedido.pid, pedido.tiempo_io);
            usleep(pedido.tiempo_io * 1000); // Convertir milisegundos a microsegundos
            log_info(io_log, "## PID: %d - Fin de IO", pedido.pid);

            // Informar al Kernel que finalizó la solicitud
            int respuesta = 1; // hay que usar algun struct respuesta?
            if (send(fd_kernel_io, &respuesta, sizeof(int), 0) <= 0) {
                log_error(io_log, "Error al informar fin de IO al Kernel: %s", strerror(errno));
            }
        } else if (bytes_recibidos == 0) {
            log_warning(io_log, "El Kernel se desconectó.");
            break; 
        } else {
            log_error(io_log, "Error al recibir petición del Kernel: %s", strerror(errno));
        }
    };
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
