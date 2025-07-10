#include "../headers/main.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"
#include "../headers/mmu.h"
#include <signal.h>
t_log* log_cpu;
pthread_t hilo_dispatch, hilo_interrupt, hilo_memoria;
pthread_mutex_t mutex_estado_proceso = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tlb = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cache = PTHREAD_MUTEX_INITIALIZER;

// Manejador de señales para terminación limpia
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n\nRecibida señal de terminación. Cerrando CPU...\n");
        log_trace(cpu_log, "Recibida señal SIGINT. Iniciando terminación limpia del CPU...");
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

    log_trace(cpu_log, AZUL("=== Iniciando CPU: %d ==="), numero_cpu);

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
    log_trace(cpu_log, "[DISPATCH] Hilo de recepción de Kernel Dispatch iniciado");
    int noFinalizar = 0;
    int cod_op;
    while (noFinalizar != -1) {
        log_trace(cpu_log, AZUL("[DISPATCH]")" Esperando operación desde Kernel...");
        cod_op = recibir_operacion(fd_kernel_dispatch);
        log_trace(cpu_log, AZUL("[DISPATCH]")" Operación recibida desde Kernel: %d", cod_op);

        switch (cod_op) {
            case MENSAJE_OP:
                log_trace(cpu_log, AZUL("[DISPATCH]")" MENSAJE_OP recibido desde Kernel");
			    recibir_mensaje(fd_kernel_dispatch, cpu_log);
			    break;
            case EXEC_OP:
                log_trace(cpu_log, AZUL("[DISPATCH]")" EXEC_OP recibido desde Kernel");
                
                // Recibir PC y PID usando recibir_contenido_paquete (con prefijo de tamaño)
                t_list* lista = recibir_contenido_paquete(fd_kernel_dispatch);
                if (lista == NULL) {
                    log_error(cpu_log, AZUL("[DISPATCH]")" Error al recibir paquete EXEC_OP");
                    break;
                }

                if (list_size(lista) < 2) {
                    log_error(cpu_log, AZUL("[DISPATCH]")" Paquete EXEC_OP incompleto - Faltan datos");
                    list_destroy_and_destroy_elements(lista, free);
                    break;
                }
                pthread_mutex_lock(&mutex_estado_proceso);
                pc = *(int*)list_get(lista, 0);         // Primer elemento = PC
                pid_ejecutando = *(int*)list_get(lista, 1);  // Segundo elemento = PID
                pthread_mutex_unlock(&mutex_estado_proceso);
                
                log_trace(cpu_log, AZUL("[DISPATCH]") " Proceso asignado - PID: %d, PC inicial: %d", pid_ejecutando, pc);
                log_trace(cpu_log, AZUL("[DISPATCH]") " Iniciando ejecución del proceso...");
                
                ejecutar_ciclo_instruccion();
                
                log_trace(cpu_log, "[DISPATCH] Ejecución del proceso PID %d finalizada", pid_ejecutando);
                // Resetear variables después de la ejecución
                pthread_mutex_lock(&mutex_estado_proceso);
                pid_ejecutando = -1;
                pc = 1;
                pthread_mutex_unlock(&mutex_estado_proceso);
                list_destroy_and_destroy_elements(lista, free);
                break;
            case SHUTDOWN_OP:
                log_info(cpu_log, "SHUTDOWN_OP recibido - Finalizando CPU");
                terminar_programa();
                exit(EXIT_SUCCESS);
                break;
            case -1:
                log_warning(cpu_log, "Se desconectó el Kernel. Finalizando CPU...");
                terminar_programa();
                exit(EXIT_SUCCESS);
            default:
                log_error(cpu_log, "[DISPATCH] Operación desconocida de Dispatch: %d", cod_op);
        }
    }
    return NULL;
}

void* recibir_kernel_interrupt(void* arg) {
    while (1) {
        int cod_op = recibir_operacion(fd_kernel_interrupt);
        switch (cod_op) {
            case -1:
                log_warning(cpu_log, "Se desconectó el Kernel. Finalizando CPU...");
                terminar_programa();
                exit(EXIT_SUCCESS);
            case INTERRUPCION_OP:
                // Recibir PID de la interrupción
                log_trace(cpu_log, VERDE("[INTERRUPT]: Recibiendo interrupción desde Kernel"));

                t_list* datos = recibir_contenido_paquete(fd_kernel_interrupt);
                if (list_size(datos) < 1) {
                    log_error(cpu_log, "[INTERRUPT]: No se recibió PID válido en interrupción");
                    list_destroy_and_destroy_elements(datos, free);
                    break;
                }

                pid_interrupt = *(int*)list_get(datos, 0);
                list_destroy_and_destroy_elements(datos, free);

                log_debug(cpu_log, VERDE("[INTERRUPT]: ## PID recibido para interrupción: %d mientras ejecuta PID %d"), pid_interrupt, pid_ejecutando);

                pthread_mutex_lock(&mutex_estado_proceso);
                hay_interrupcion = 1;
                pthread_mutex_unlock(&mutex_estado_proceso);
                break;
            default:
                log_error(cpu_log, "Operacion desconocida de Interrupt: %d", cod_op);
        }
    }
}

void iterator(char* value) {
    log_trace(cpu_log, "%s", value);
}

void terminar_programa() {
    log_trace(cpu_log, "Iniciando terminación limpia del CPU...");
    
    // Cerrar conexiones de sockets
    if (fd_kernel_dispatch > 0) {
        log_trace(cpu_log, "Conexión Kernel Dispatch cerrada");
        close(fd_kernel_dispatch);
    }
    
    if (fd_kernel_interrupt > 0) {
        log_trace(cpu_log, "Conexión Kernel Interrupt cerrada");
        close(fd_kernel_interrupt);
    }
    
    if (fd_memoria > 0) {
        log_trace(cpu_log, "Conexión Memoria cerrada");
        close(fd_memoria);
    }
    
    if (cfg_memoria != NULL) {
        log_trace(cpu_log, "Configuración MEMORIA liberada");
        free(cfg_memoria);
    }
    
    if (cpu_log != NULL) {
        log_trace(cpu_log, "CPU terminado correctamente");
        log_destroy(cpu_log);
    }

    if (cpu_config != NULL) {
        config_destroy(cpu_config);
    }
}
