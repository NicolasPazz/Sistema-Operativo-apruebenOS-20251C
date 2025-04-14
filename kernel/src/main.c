#include "../headers/kernel.h"

void* hilo_conexiones(void* _) {
    iniciar_conexiones_kernel();
    return NULL;
}

int main(int argc, char* argv[]) {
    iniciar_config_kernel();
    iniciar_logger_kernel();
    //pthread_t hilo_servidor;
    //pthread_create(&hilo_servidor, NULL, hilo_conexiones, NULL);

    

    while(1);
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
    log_info(kernel_log, "%s", value);
}
