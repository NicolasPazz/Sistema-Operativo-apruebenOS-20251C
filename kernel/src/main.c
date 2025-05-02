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
      // Servidor de CPU
    pthread_t hilo_dispatch;
    pthread_create(&hilo_dispatch, NULL, hilo_servidor_dispatch, NULL);
    pthread_detach(hilo_dispatch);

    pthread_t hilo_interrupt;
    pthread_create(&hilo_interrupt, NULL, hilo_servidor_interrupt, NULL);
    pthread_detach(hilo_interrupt);

      // Cliente de Memoria
    pthread_t hilo_memoria;
    pthread_create(&hilo_memoria, NULL, hilo_cliente_memoria, NULL);
    pthread_detach(hilo_memoria);

      // Servidor de IO
    pthread_t hilo_io;
    pthread_create(&hilo_io, NULL, hilo_servidor_io, NULL);
    pthread_detach(hilo_io);  

    //////////////////////////// Esperar conexiones minimas ////////////////////////////
    log_info(kernel_log, "Esperando conexion con al menos una CPU y una IO");

    while (true) {
        pthread_mutex_lock(&mutex_conexiones);
        if (conectado_cpu && conectado_io) break;
        pthread_mutex_unlock(&mutex_conexiones);
        sleep(1);
    }

    log_info(kernel_log, "CPU y IO conectados. Continuando ejecucion");
  
    //////////////////////////// Primer proceso ////////////////////////////  
    printf("\n\n\n");
    mostrar_colas_estados();

    log_info(kernel_log, "Creando proceso inicial:  Archivo: %s, Tamaño: %d", archivo_pseudocodigo, tamanio_proceso);
    INIT_PROC(archivo_pseudocodigo, tamanio_proceso);

    mostrar_colas_estados();

    printf("\nPresione ENTER para iniciar planificación...\n");

    //////////////////////////// Planificacion ////////////////////////////
    int c = getchar();
    while (c != '\n') {
        fprintf(stderr, "Error: Debe presionar solo ENTER para continuar.\n");

        while ((c = getchar()) != '\n' && c != EOF);

        printf("\nPresione ENTER para iniciar planificación...\n");
        c = getchar();
    }
    
    // Iniciar planificacion de largo plazo

    // Iniciar planificacion de mediano plazo

    // Iniciar planificacion de corto plazo

    //////////////////////////// Test ////////////////////////////
    printf("Creando 2 procesos más... \n");
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
    
    return EXIT_SUCCESS;
  
}

void iterator(char* value) {
    log_info(kernel_log, "%s", value);
}
