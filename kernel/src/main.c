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
  
    //////////////////////////// Primer Proceso ////////////////////////////
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [archivo_pseudocodigo] [tamanio_proceso]\nEJ: ./bin/kernel PROCESO_INICIAL 128\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* archivo_pseudocodigo = argv[1];
    int tamanio_proceso = atoi(argv[2]);
  
    //////////////////////////// Config, log e inicializaciones ////////////////////////////
    iniciar_sincronizacion_kernel();
    iniciar_logger_kernel_debug();
    iniciar_config_kernel();
    iniciar_logger_kernel();
    iniciar_estados_kernel();
    iniciar_diccionario_tiempos();
    iniciar_diccionario_archivos_por_pcb();
    
    //////////////////////////// Conexiones del Kernel ////////////////////////////

    // Servidor de CPU (Dispatch)
    pthread_t hilo_dispatch;
    if (pthread_create(&hilo_dispatch, NULL, hilo_servidor_dispatch, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo de servidor Dispatch");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_dispatch);
    //log_debug(kernel_log, "Hilo de servidor Dispatch creado correctamente");

    // Servidor de CPU (Interrupt)
    pthread_t hilo_interrupt;
    if (pthread_create(&hilo_interrupt, NULL, hilo_servidor_interrupt, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo de servidor Interrupt");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_interrupt);
    //log_debug(kernel_log, "Hilo de servidor Interrupt creado correctamente");

    // Cliente de Memoria
    pthread_t hilo_memoria;
    if (pthread_create(&hilo_memoria, NULL, hilo_cliente_memoria, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo cliente de Memoria");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_memoria);
    //log_debug(kernel_log, "Hilo cliente de Memoria creado correctamente");

    // Servidor de IO
    pthread_t hilo_io;
    if (pthread_create(&hilo_io, NULL, hilo_servidor_io, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo de servidor IO");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_io);
    //log_debug(kernel_log, "Hilo de servidor IO creado correctamente");


    //////////////////////////// Esperar conexiones minimas ////////////////////////////
    log_debug(kernel_log, "Esperando conexion con al menos una CPU y una IO");

    while (true) {
        pthread_mutex_lock(&mutex_conexiones);
        if (conectado_cpu && conectado_io) break;
        pthread_mutex_unlock(&mutex_conexiones);
        sleep(1);
    }

    log_debug(kernel_log, "CPU y IO conectados. Continuando ejecucion");
  
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
    
    //////////////////////////// Primer proceso ////////////////////////////  
    printf("\n\n\n");
    mostrar_colas_estados();

    log_debug(kernel_log, "Creando proceso inicial:  Archivo: %s, Tamanio: %d", archivo_pseudocodigo, tamanio_proceso);
    INIT_PROC(archivo_pseudocodigo, tamanio_proceso);
    
    loguear_metricas_estado(list_get(cola_new, 0));
    mostrar_colas_estados();

    //////////////////////////// Planificacion ////////////////////////////
    iniciar_planificador_largo_plazo();

    //////////////////////////// Mantener el kernel ejecutandose ////////////////////////////
    log_info(kernel_log, "Kernel iniciado correctamente. Planificadores en ejecucion...");
    printf("Kernel ejecutandose. Presione Ctrl+C para terminar.\n");
    
    // Mantener el programa principal ejecutándose
    while (1) {
        sleep(10); // Dormir para no consumir CPU innecesariamente
        // Aquí podrías agregar chequeos periódicos si fuera necesario
    }

    //////////////////////////// Terminar ////////////////////////////  
    terminar_kernel();

    return EXIT_SUCCESS;
  
}

void iterator(char* value) {
    log_debug(kernel_log, "%s", value);
}
