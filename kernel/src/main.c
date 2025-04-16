#include "../headers/kernel.h"

void* hilo_conexiones(void* _) {
    iniciar_conexiones_kernel();
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [archivo_pseudocodigo] [tamanio_proceso]\nEJ: ./bin/kernel kernel/script/proceso_inicial.pseudo 128\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* archivo_pseudocodigo = argv[1];
    int tamanio_proceso = atoi(argv[2]);

    iniciar_logger_kernel_debug();
    iniciar_config_kernel();
    iniciar_logger_kernel();
    iniciar_estados_kernel();

    //pthread_t hilo_servidor;
    //pthread_create(&hilo_servidor, NULL, hilo_conexiones, NULL);

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

    int c = getchar();
    while (c != '\n') {
        fprintf(stderr, "Error: Debe presionar solo ENTER para continuar.\n");

        while ((c = getchar()) != '\n' && c != EOF);

        printf("\nPresione ENTER para iniciar planificación...\n");
        c = getchar();
    }


    //inicializar planificacion de largo plazo
    
    // TEST
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
