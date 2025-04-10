#include "../headers/kernel.h"

int main(int argc, char* argv[]) {
    iniciar_config_kernel();
    iniciar_logger_kernel();
    iniciar_conexiones_kernel();


    /* TODO Envio de mensajes
    //////////////////////////// Estructuras de datos para atender clientes ////////////////////////////
    cliente_data_t *data_memoria = crear_cliente_data(fd_memoria, kernel_log, "Memoria");

    //////////////////////////// Atender Memoria ////////////////////////////
    pthread_t hilo_memoria;
    pthread_create(&hilo_memoria, NULL, (void*)atender_cliente, (void*)data_memoria);
    pthread_join(hilo_memoria, NULL);
    paquete(fd_memoria);
    */
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
