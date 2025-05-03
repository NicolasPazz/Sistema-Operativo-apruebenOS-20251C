#include "../headers/main.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"

pthread_t hilo_dispatch, hilo_interrupt, hilo_memoria;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "[CPU] Uso: %s <ID_CPU>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    uint32_t numero_cpu = atoi(argv[1]);
    leer_config_cpu();
    iniciar_logger_cpu();

    log_info(cpu_log, "[CPU %i] Iniciando proceso CPU", numero_cpu);

    conectar_kernel_dispatch();
    send(fd_kernel_dispatch, &numero_cpu, sizeof(uint32_t), 0);

    conectar_kernel_interrupt();
    send(fd_kernel_interrupt, &numero_cpu, sizeof(uint32_t), 0);

    conectar_cpu_memoria();

    pthread_t atiende_respuestas_kernel_dispatch;
    pthread_create(&atiende_respuestas_kernel_dispatch, NULL, (void *)recibir_kernel_dispatch, (void *)(intptr_t)fd_kernel_dispatch);
    pthread_detach(atiende_respuestas_kernel_dispatch);

    pthread_t atiende_respuestas_kernel_interrupt;
    pthread_create(&atiende_respuestas_kernel_interrupt, NULL, (void *)recibir_kernel_interrupt, (void *)(intptr_t)fd_kernel_interrupt);
    pthread_detach(atiende_respuestas_kernel_interrupt);

    //ejecutar_ciclo_instruccion();
    
    terminar_programa();
    return EXIT_SUCCESS;  
}

// NO UTIL POR EL MOMENTOS
// void atender_cliente(void* arg) { 
//     cliente_data_t *data = (cliente_data_t *)arg;
//     int control_key = 1;
//     while (control_key) {
//         int cod_op = recibir_operacion(data->fd);
//         switch (cod_op) {
//             case MENSAJE:
//                 recibir_mensaje(data->fd, data->logger);
//                 break;
//             case PAQUETE:
//                 t_list* lista = recibir_paquete(data->fd);
//                 list_iterate(lista, (void*)iterator);
//                 list_destroy(lista);
//                 break;
//             case -1:
//                 log_error(data->logger, "El cliente (%s) se desconectó. Terminando servidor.", data->cliente);
//                 control_key = 0;
//                 break;
//             default:
//                 log_warning(data->logger, "Operación desconocida de %s", data->cliente);
//                 break;
//         }
//     }
// }

int recibir_kernel_dispatch(int fd_kernel_dispatch) {
    int noFinalizar = 0;
    while (noFinalizar != -1) {
        int cod_op = recibir_operacion(fd_kernel_dispatch);
        switch (cod_op) {
            case MENSAJE_OP:
			    recibir_mensaje(fd_kernel_dispatch, cpu_log);
			    break;
            case EXEC_OP:
                //int pid_exec = recibir_pid(fd_kernel_dispatch);
                //int pid = recibir_pid(fd_kernel_dispatch);
                int pc, pid;//= recibir_instruccion(fd_kernel_dispatch);
                //log_info(cpu_log, "EXEC - PID: %d, Instrucción: %s", pid, instruccion->parametros1);
                // Ejecutar la instrucción
                /*t_list* lista = recibir_2_enteros(fd_kernel_dispatch);
                pc = (int)(intptr_t) list_get(lista, 0);
                pid = (int)(intptr_t) list_get(lista, 1);*/
                ejecutar_ciclo_instruccion(pc, pid);
                break;
            case -1:
                log_error(cpu_log, "Desconexión de Kernel (Dispatch)");
                close(fd_kernel_dispatch);
                return NULL;    // warning: returning ‘void *’ from a function with return type ‘int’
            default:
                log_warning(cpu_log, "Operación desconocida de Dispatch: %d", cod_op);
        }
    }
}

int recibir_kernel_interrupt(int fd_kernel_interrupt) {
    while (1) {
        int cod_op = recibir_operacion(fd_kernel_interrupt);
        switch (cod_op) {
            case -1:
                log_error(cpu_log, "Desconexión de Kernel (Interrupt)");
                close(fd_kernel_interrupt);
                return NULL;    // warning: returning ‘void *’ from a function with return type ‘int’
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
