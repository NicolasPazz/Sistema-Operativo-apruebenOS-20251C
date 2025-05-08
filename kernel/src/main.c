#include "../headers/kernel.h"

int main(int argc, char* argv[]) {
  
    //////////////////////////// Primer Proceso ////////////////////////////
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [archivo_pseudocodigo] [tamanio_proceso]\nEJ: ./bin/kernel kernel/script/proceso_inicial.pseudo 128\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* archivo_pseudocodigo = argv[1];
    int tamanio_proceso = atoi(argv[2]);
  
    //////////////////////////// Config, log e inicializaciones ////////////////////////////
    iniciar_logger_kernel_debug();
    iniciar_config_kernel();
    iniciar_logger_kernel();
    iniciar_estados_kernel();
    iniciar_diccionario_tiempos();
    iniciar_sincronizacion_kernel();

    //////////////////////////// Conexiones del Kernel ////////////////////////////

    // Servidor de CPU (Dispatch)
    pthread_t hilo_dispatch;
    if (pthread_create(&hilo_dispatch, NULL, hilo_servidor_dispatch, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo de servidor Dispatch");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_dispatch);
    log_debug(kernel_log, "Hilo de servidor Dispatch creado correctamente");

    // Servidor de CPU (Interrupt)
    pthread_t hilo_interrupt;
    if (pthread_create(&hilo_interrupt, NULL, hilo_servidor_interrupt, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo de servidor Interrupt");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_interrupt);
    log_debug(kernel_log, "Hilo de servidor Interrupt creado correctamente");

    // Cliente de Memoria
    pthread_t hilo_memoria;
    if (pthread_create(&hilo_memoria, NULL, hilo_cliente_memoria, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo cliente de Memoria");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_memoria);
    log_debug(kernel_log, "Hilo cliente de Memoria creado correctamente");

    // Servidor de IO
    pthread_t hilo_io;
    if (pthread_create(&hilo_io, NULL, hilo_servidor_io, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo de servidor IO");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_io);
    log_debug(kernel_log, "Hilo de servidor IO creado correctamente");


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

    printf("\nPresione ENTER para iniciar planificacion...\n");
    
    int c = getchar();
    while (c != '\n') {
        fprintf(stderr, "Error: Debe presionar solo ENTER para continuar.\n");

        while ((c = getchar()) != '\n' && c != EOF);

        printf("\nPresione ENTER para iniciar planificacion...\n");
        c = getchar();
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

    //////////////////////////// Test ////////////////////////////
    log_debug(kernel_log, "Creando 2 procesos mas... \n");
    INIT_PROC("Test2", 11);
    INIT_PROC("Test2", 12);

    mostrar_colas_estados(); // 3 Procesos en new

    mostrar_pcb(*(t_pcb*)list_get(cola_procesos, 0));
    mostrar_pcb(*(t_pcb*)list_get(cola_procesos, 1));
    mostrar_pcb(*(t_pcb*)list_get(cola_procesos, 2));
    
    cambiar_estado_pcb((t_pcb*)list_get(cola_procesos, 0), READY);
    cambiar_estado_pcb((t_pcb*)list_get(cola_procesos, 1), READY);
    cambiar_estado_pcb((t_pcb*)list_get(cola_procesos, 2), READY);
    mostrar_colas_estados(); // 3 Procesos en READY

    cambiar_estado_pcb((t_pcb*)list_get(cola_procesos, 0), EXEC);
    sleep(3);

    cambiar_estado_pcb((t_pcb*)list_get(cola_procesos, 1), EXEC);
    sleep(1);

    cambiar_estado_pcb((t_pcb*)list_get(cola_procesos, 2), EXEC);
    sleep(4);

    t_pcb* pid0 = (t_pcb*)list_get(cola_procesos, 0);
    t_pcb* pid1 = (t_pcb*)list_get(cola_procesos, 1);
    t_pcb* pid2 = (t_pcb*)list_get(cola_procesos, 2);

    fin_io(pid2);
    fin_io(pid1);
    fin_io(pid0);
    
    //////////////////////////// Planificacion de corto plazo ////////////////////////////
    iniciar_planificador_corto_plazo(ALGORITMO_CORTO_PLAZO);

    mostrar_colas_estados(); // PID 0 en EXEC

    mostrar_pcb(*(t_pcb*)list_get(cola_procesos, 0));
    mostrar_pcb(*(t_pcb*)list_get(cola_procesos, 1));
    mostrar_pcb(*(t_pcb*)list_get(cola_procesos, 2));
    

    //////////////////////////// Terminar ////////////////////////////  
    terminar_kernel();
    
    while(1){
      sleep(100);
    };
    return EXIT_SUCCESS;
  
}

void iterator(char* value) {
    log_debug(kernel_log, "%s", value);
}
