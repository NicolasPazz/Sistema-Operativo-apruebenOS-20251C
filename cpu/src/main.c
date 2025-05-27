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

    ejecutar_ciclo_instruccion();
    
    //provisorio para que no finalice
    while (1) {
        sleep(1);
    }
    
    terminar_programa();
    return EXIT_SUCCESS;  
}  

void* recibir_kernel_dispatch(void* arg) {
    while (1) {
        op_code cod_op = recibir_operacion(fd_kernel_dispatch);
        switch (cod_op) {
            case MENSAJE_OP:
			        recibir_mensaje(fd_kernel_dispatch, cpu_log);
			    break;
            case EXEC_OP:
                // Recibir PC y PID
                    int pc, pid_ejecutando;
                    recibir_2_enteros(fd_kernel_dispatch, &pc, &pid_ejecutando);

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
