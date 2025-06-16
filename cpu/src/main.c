#include "../headers/main.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"
#include "../headers/mmu.h"
#include <signal.h>

pthread_t hilo_dispatch, hilo_interrupt, hilo_memoria;

// Manejador de señales para terminación limpia
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n\nRecibida señal de terminación. Cerrando CPU...\n");
        log_info(cpu_log, "Recibida señal SIGINT. Iniciando terminación limpia del CPU...");
        terminar_programa();
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char* argv[]) {
    // Configurar el manejador de señales
    signal(SIGINT, signal_handler);
    
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

    // CAMBIO: Comentamos esta línea para que CPU no ejecute prematuramente
    // ejecutar_ciclo_instruccion();
    
    //provisorio para que no finalice
    while (1) {
        sleep(1);
    }
    
    terminar_programa();
    return EXIT_SUCCESS;  
}  

void* recibir_kernel_dispatch(void* arg) {
    log_info(cpu_log, "[DISPATCH] Hilo de recepción de Kernel Dispatch iniciado");
    int noFinalizar = 0;
    while (noFinalizar != -1) {
        log_debug(cpu_log, "[DISPATCH] Esperando operación desde Kernel...");
        int cod_op = recibir_operacion(fd_kernel_dispatch);
        log_debug(cpu_log, "[DISPATCH] Operación recibida desde Kernel: %d", cod_op);
        
        switch (cod_op) {
            case MENSAJE_OP:
                log_debug(cpu_log, "[DISPATCH] Procesando MENSAJE_OP");
			    recibir_mensaje(fd_kernel_dispatch, cpu_log);
			    break;
            case EXEC_OP:
                log_info(cpu_log, "[DISPATCH] ✓ EXEC_OP recibido desde Kernel - Iniciando ejecución");
                // Ejecutar la instrucción
                t_list* lista = recibir_2_enteros_sin_op(fd_kernel_dispatch);
                pc = (int)(intptr_t) list_get(lista, 0);
                pid_ejecutando = (int)(intptr_t) list_get(lista, 1);
                
                log_info(cpu_log, "[DISPATCH] ✓ Proceso asignado - PID: %d, PC inicial: %d", pid_ejecutando, pc);
                log_info(cpu_log, "[DISPATCH] ▶ Iniciando ejecución del proceso...");
                
                ejecutar_ciclo_instruccion();
                
                log_info(cpu_log, "[DISPATCH] ◼ Ejecución del proceso PID %d finalizada", pid_ejecutando);
                // Resetear variables después de la ejecución
                pid_ejecutando = -1;
                pc = 0;
                break;
            case -1:
                log_error(cpu_log, "[DISPATCH] ✗ Desconexión de Kernel (Dispatch)");
                close(fd_kernel_dispatch);
                break;
            default:
                log_error(cpu_log, "[DISPATCH] ✗ Operación desconocida de Dispatch: %d", cod_op);
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
    log_info(cpu_log, "Iniciando terminación limpia del CPU...");
    
    // Cerrar conexiones de sockets
    if (fd_kernel_dispatch > 0) {
        close(fd_kernel_dispatch);
        log_debug(cpu_log, "Conexión Kernel Dispatch cerrada");
    }
    
    if (fd_kernel_interrupt > 0) {
        close(fd_kernel_interrupt);
        log_debug(cpu_log, "Conexión Kernel Interrupt cerrada");
    }
    
    if (fd_memoria > 0) {
        close(fd_memoria);
        log_debug(cpu_log, "Conexión Memoria cerrada");
    }
    
    // Liberar recursos de configuración y logging
    if (cpu_config != NULL) {
        config_destroy(cpu_config);
        log_debug(cpu_log, "Configuración CPU liberada");
    }
    
    if (cpu_log != NULL) {
        log_info(cpu_log, "CPU terminado correctamente");
        log_destroy(cpu_log);
    }
}
