#include "../headers/kernel.h"

int main(int argc, char* argv[]) {
  
    //////////////////////////// Primer Proceso ////////////////////////////
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [archivo_pseudocodigo] [tamanio_proceso]\nEJ: ./bin/kernel kernel/script/proceso_inicial.pseudo 128\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* archivo_pseudocodigo = argv[1];
    int tamanio_proceso = atoi(argv[2]);
  
    //////////////////////////// Config y log ////////////////////////////
    iniciar_logger_kernel_debug();
    iniciar_config_kernel();
    iniciar_logger_kernel();
    iniciar_estados_kernel();

    //////////////////////////// Conexiones del Kernel ////////////////////////////
    pthread_t hilo_dispatch;
    pthread_create(&hilo_dispatch, NULL, hilo_servidor_dispatch, NULL);
    pthread_detach(hilo_dispatch);

    pthread_t hilo_interrupt;
    pthread_create(&hilo_interrupt, NULL, hilo_servidor_interrupt, NULL);
    pthread_detach(hilo_interrupt);

    pthread_t hilo_memoria;
    pthread_create(&hilo_memoria, NULL, hilo_cliente_memoria, NULL);
    pthread_detach(hilo_memoria);

    pthread_t hilo_io;
    pthread_create(&hilo_io, NULL, hilo_servidor_io, NULL);
    pthread_join(hilo_io, NULL);
  
    //////////////////////////// Primer proceso ////////////////////////////  
    printf("Cola NEW: %d\n", list_size(cola_new));
    printf("Cola READY: %d\n", list_size(cola_ready));
    printf("Cola procesos totales: %d\n", list_size(cola_procesos));

    log_info(kernel_log, "Creando proceso inicial:  Archivo: %s, Tamaño: %d", archivo_pseudocodigo, tamanio_proceso);
    INIT_PROC(archivo_pseudocodigo, tamanio_proceso);

    mostrar_pcb(*(t_pcb*)list_get(cola_new, 0));
    printf("Cola NEW: %d\n", list_size(cola_new));
    printf("Cola READY: %d\n", list_size(cola_ready));
    printf("Cola procesos totales: %d\n", list_size(cola_procesos));

    printf("\nPresione ENTER para iniciar planificación...\n");

    //////////////////////////// Planificacion de largo plazo ////////////////////////////
    int c = getchar();
    while (c != '\n') {
        fprintf(stderr, "Error: Debe presionar solo ENTER para continuar.\n");

        while ((c = getchar()) != '\n' && c != EOF);

        printf("\nPresione ENTER para iniciar planificación...\n");
        c = getchar();
    }
    
    // Iniciar planificacion de largo plazo

    //////////////////////////// Test ////////////////////////////
    printf("Creando 2 procesos más... \n");
    INIT_PROC("Test2", 11);
    INIT_PROC("Test2", 12);
    printf("Cola NEW: %d\n", list_size(cola_new));
    printf("Cola READY: %d\n", list_size(cola_ready));
    printf("Cola procesos totales: %d\n", list_size(cola_procesos));
    mostrar_pcb(*(t_pcb*)list_get(cola_new, 0));
    mostrar_pcb(*(t_pcb*)list_get(cola_new, 1));
    mostrar_pcb(*(t_pcb*)list_get(cola_new, 2));

    cambiar_estado_pcb((t_pcb*)list_get(cola_new, 0), READY);
    
    printf("Cola NEW: %d\n", list_size(cola_new));
    printf("Cola READY: %d\n", list_size(cola_ready));
    printf("Cola procesos totales: %d\n", list_size(cola_procesos));
    mostrar_pcb(*(t_pcb*)list_get(cola_new, 0));
    mostrar_pcb(*(t_pcb*)list_get(cola_new, 1));
    mostrar_pcb(*(t_pcb*)list_get(cola_ready, 0));
  
    return EXIT_SUCCESS;
  
}

void iterator(char* value) {
    log_info(kernel_log, "%s", value);
}
