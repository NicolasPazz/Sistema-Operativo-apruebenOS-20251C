#include "../headers/cpu.h"

int main(int argc, char* argv[]) {
    iniciar_config_cpu();
    iniciar_logger_cpu();
    iniciar_conexiones_cpu();
    return EXIT_SUCCESS;
}

void atender_cliente(void* arg) {
    cliente_data_t *data = (cliente_data_t *)arg;
    int control_key = 1;
    while (control_key) {
        int cod_op = recibir_operacion(data->fd);
        switch (cod_op) {
            case MENSAJE:
                recibir_mensaje(data->fd, data->logger);
                break;
            case PAQUETE:
                t_list* lista = recibir_paquete(data->fd);
                list_iterate(lista, (void*)iterator);
                list_destroy(lista);
                break;
            case -1:
                log_error(data->logger, "El cliente (%s) se desconectó. Terminando servidor.", data->cliente);
                control_key = 0;
                break;
            default:
                log_warning(data->logger, "Operación desconocida de %s", data->cliente);
                break;
        }
    }
}

void iterator(char* value) {
    log_info(cpu_log, "%s", value);
}