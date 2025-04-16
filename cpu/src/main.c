#include "../headers/main.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"

pthread_t hilo_dispatch, hilo_interrupt;

int main(int argc, char* argv[]) {
    
    printf("INICIA EL MODULO DE CPU");
    leer_config_cpu();    
    iniciar_logger_cpu();

    conectar_cpu_memoria();
    conectar_kernel_dispatch();
    conectar_kernel_interrupt();

    pthread_create(&hilo_dispatch, NULL, recibir_kernel_dispatch, NULL);
    pthread_create(&hilo_interrupt, NULL, recibir_kernel_interrupt, NULL);

    //ejecutar_ciclo_instruccion();
    terminar_programa();
    return EXIT_SUCCESS;  
}

// NO UTIL POR EL MOMENTOS
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

void* recibir_kernel_dispatch(void* arg) {
    while (1) {
        int cod_op = recibir_operacion(fd_kernel_dispatch);
        switch (cod_op) {
            case EXEC:
                //int pid = recibir_pid(fd_kernel_dispatch);
                int pc, pid;//= recibir_instruccion(fd_kernel_dispatch);
                //log_info(cpu_log, "EXEC - PID: %d, Instrucción: %s", pid, instruccion->parametros1);
                // Ejecutar la instrucción
                ejecutar_ciclo_instruccion(pc, pid);
                break;
            case -1:
                log_error(cpu_log, "Desconexión de Kernel (Dispatch)");
                close(fd_kernel_dispatch);
                return NULL;
            default:
                log_warning(cpu_log, "Operación desconocida de Dispatch: %d", cod_op);
        }
    }
}

void* recibir_kernel_interrupt(void* arg) {
    while (1) {
        int cod_op = recibir_operacion(fd_kernel_interrupt);
        switch (cod_op) {
            case -1:
                log_error(cpu_log, "Desconexión de Kernel (Interrupt)");
                close(fd_kernel_interrupt);
                return NULL;
            default:
                log_warning(cpu_log, "Operación desconocida de Interrupt: %d", cod_op);
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
