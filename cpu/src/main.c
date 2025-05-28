#include "../headers/main.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"
#include "../headers/mmu.h"
pthread_t hilo_dispatch, hilo_interrupt, hilo_memoria;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "[CPU] Uso: %s <ID_CPU>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int numero_cpu = atoi(argv[1]);
    leer_config_cpu();
    iniciar_logger_cpu();
    inicializar_mmu();

    log_debug(cpu_log, "[CPU %i] Iniciando proceso CPU", numero_cpu);

    conectar_kernel_dispatch();
    send(fd_kernel_dispatch, &numero_cpu, sizeof(int), 0);

    conectar_kernel_interrupt();
    send(fd_kernel_interrupt, &numero_cpu, sizeof(int), 0);

    conectar_cpu_memoria();

    pthread_t atiende_respuestas_kernel_dispatch;
    pthread_create(&atiende_respuestas_kernel_dispatch, NULL, (void *) recibir_kernel_dispatch, (void*)& fd_kernel_dispatch);
    pthread_detach(atiende_respuestas_kernel_dispatch);

    pthread_t atiende_respuestas_kernel_interrupt;
    pthread_create(&atiende_respuestas_kernel_interrupt, NULL, (void *) recibir_kernel_interrupt, (void*)& fd_kernel_interrupt);
    pthread_detach(atiende_respuestas_kernel_interrupt);
//
    ejecutar_ciclo_instruccion();
    
    //provisorio para que no finalice
    while (1) {
        sleep(1);
    }
    
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
//                 log_error(data->logger, "El cliente (%s) se desconecto. Terminando servidor.", data->cliente);
//                 control_key = 0;
//                 break;
//             default:
//                 log_error(data->logger, "Operacion desconocida de %s", data->cliente);
//                 break;
//         }
//     }
// }    

void* recibir_kernel_dispatch(void* arg) {
    int noFinalizar = 0;
    while (noFinalizar != -1) {
        int cod_op = recibir_operacion(fd_kernel_dispatch);
        switch (cod_op) {
            case MENSAJE_OP:
			    recibir_mensaje(fd_kernel_dispatch, cpu_log);
			    break;
            case EXEC_OP:
                // Ejecutar la instrucción
                t_list* lista = recibir_2_enteros(fd_kernel_dispatch);
                pc = (int)(intptr_t) list_get(lista, 0);
                pid_ejecutando = (int)(intptr_t) list_get(lista, 1);
                ejecutar_ciclo_instruccion();
                break;
            case -1:
                log_error(cpu_log, "Desconexion de Kernel (Dispatch)");
                close(fd_kernel_dispatch);
            default:
            log_error(cpu_log, "Operacion desconocida de Dispatch: %d", cod_op);
        }
    }
    return NULL;
}

void* recibir_kernel_interrupt(void* arg) {
    while (1) {
        int cod_op = recibir_operacion(fd_kernel_interrupt);
        switch (cod_op) {
            case -1:
                log_warning(cpu_log, "Se desconectó el Kernel (Dispatch). Finalizando CPU...");
                terminar_programa();
            case INTERRUPCION_OP:
                // Recibir PID de la interrupción
                recv(fd_kernel_interrupt, &pid_interrupt, sizeof(int), MSG_WAITALL);
                log_info(cpu_log, "Recibida interrupción para PID: %d", pid_interrupt);
                
                hay_interrupcion = 1;
                break;
            default:
                log_error(cpu_log, "Operacion desconocida de Interrupt: %d", cod_op);
        }
    }
}

void iterator(char* value) {
    log_debug(cpu_log, "%s", value);
}


void terminar_programa() {
    log_destroy(cpu_log);
    config_destroy(cpu_config);
}
