#include "../headers/kernel.h"

// Manejador de señales para terminación limpia
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n\nRecibida señal de terminación. Cerrando kernel...\n");
        terminar_kernel();
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char* argv[]) {

    signal(SIGINT, signal_handler);
  
    //////////////////////////// Config, log e inicializaciones ////////////////////////////
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [archivo_pseudocodigo] [tamanio_proceso]\nEJ: ./bin/kernel proceso_inicial 128\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    iniciar_sincronizacion_kernel();
    iniciar_config_kernel();
    iniciar_logger_kernel();
    iniciar_estados_kernel();
    iniciar_diccionario_tiempos();
    iniciar_diccionario_archivos_por_pcb();
    iniciar_planificadores();
    if(strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0) iniciar_interrupt_handler();

    char* archivo_pseudocodigo = argv[1];
    int tamanio_proceso = atoi(argv[2]);
       
    //////////////////////////// Conexiones del Kernel ////////////////////////////

    // Servidor de CPU (Dispatch)
    pthread_t hilo_dispatch;
    if (pthread_create(&hilo_dispatch, NULL, hilo_servidor_dispatch, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo de servidor Dispatch");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_dispatch);
    log_trace(kernel_log, "Hilo de servidor Dispatch creado correctamente");

    // Servidor de CPU (Interrupt)
    pthread_t hilo_interrupt;
    if (pthread_create(&hilo_interrupt, NULL, hilo_servidor_interrupt, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo de servidor Interrupt");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_interrupt);
    log_trace(kernel_log, "Hilo de servidor Interrupt creado correctamente");

    // Cliente de Memoria
    pthread_t hilo_memoria;
    if (pthread_create(&hilo_memoria, NULL, hilo_cliente_memoria, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo cliente de Memoria");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_memoria);
    log_trace(kernel_log, "Hilo cliente de Memoria creado correctamente");

    // Servidor de IO
    pthread_t hilo_io;
    if (pthread_create(&hilo_io, NULL, hilo_servidor_io, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo de servidor IO");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_io);
    log_trace(kernel_log, "Hilo de servidor IO creado correctamente");

    //////////////////////////// Esperar conexiones minimas ////////////////////////////
    log_trace(kernel_log, "Esperando conexion con al menos una CPU, una IO y Memoria...");

    while (true) {
        pthread_mutex_lock(&mutex_conexiones);
        if (conectado_cpu && conectado_io && conectado_memoria){
            pthread_mutex_unlock(&mutex_conexiones);
            break;
        } 
        pthread_mutex_unlock(&mutex_conexiones);
        sleep(1);
    }

    log_trace(kernel_log, "CPU, IO y Memoria conectados. Continuando ejecucion");
    
    //////////////////////////// Primer proceso ////////////////////////////
    log_trace(kernel_log, "Creando proceso inicial:  Archivo: %s, Tamanio: %d", archivo_pseudocodigo, tamanio_proceso);
    INIT_PROC(archivo_pseudocodigo, tamanio_proceso); 

    //////////////////////////// Esperar enter ////////////////////////////

    if (argv[3] == NULL) {
        printf("\nPresione ENTER para iniciar planificacion...\n");
    
        int c = getchar();
        while (c != '\n') {
            fprintf(stderr, "Error: Debe presionar solo ENTER para continuar.\n");
            while ((c = getchar()) != '\n' && c != EOF);
            printf("\nPresione ENTER para iniciar planificacion...\n");
            c = getchar();
        }
    } else if (strcmp(argv[3], "--action") != 0) {
        log_error(kernel_log, "Parametro desconocido: %s", argv[3]);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "Kernel ejecutandose. Presione Ctrl+C para terminar.");

    activar_planificador_largo_plazo();

    while (1) {
        sleep(10);
    }

    //////////////////////////// Terminar ////////////////////////////  
    terminar_kernel();

    return EXIT_SUCCESS;
  
}

void iterator(char* value) {
    log_trace(kernel_log, "%s", value);
}
