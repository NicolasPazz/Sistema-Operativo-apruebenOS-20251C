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
        sleep(100);
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