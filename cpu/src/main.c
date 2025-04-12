#include "../headers/cpu.h"



int main(int argc, char* argv[]) {
    
    printf("INICIA EL MODULO DE CPU");
    leer_config_cpu();
    
    iniciar_logger_cpu();
    cpu_log = log_create("./cpu.log", "CPU", 1, LOG_LEVEL_TRACE);
    iniciar_conexiones_cpu(); // esta me gustaria modificarla, donde solamente haya 1 funcion con memoria y aca no se si 1 hilo para kernel o 2 hilos...
    return EXIT_SUCCESS;      // 1 para el interrupt y el otro para dispatch
    establecer_conexion_cpu_memoria();
    //ver el tema de hilos para dispatch e interrept, creo que 2 hilos no nos va a alcanzar en el caso de haber varias cpus.
    establecer_conexion_cpu_kernel();


    terminar_programa();

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


void terminar_programa() {
    log_destroy(cpu_log);
    config_destroy(cpu_config);
}
