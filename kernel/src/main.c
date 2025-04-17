#include "../headers/kernel.h"

int main(int argc, char* argv[]) {
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


    //////////////////////////// Procesos ////////////////////////////
    printf("size cola new: %d \n", list_size(cola_new));
    INIT_PROC("Test1", 10);
    printf("size cola new: %d \n", list_size(cola_new));
    printf("Creando 2 procesos más... \n");
    INIT_PROC("Test2", 11);
    INIT_PROC("Test2", 12);
    printf("size cola new: %d \n", list_size(cola_new)); // 2 procesos
    printf("size cola ready: %d \n", list_size(cola_ready)); // 0 procesos
    printf("Cantidad total procesos: %d \n", list_size(cola_procesos)); // 3 procesos
    
    mostrar_pcb(*(t_pcb*)list_get(cola_new, 2));
    cambiar_estado_pcb((t_pcb*)list_get(cola_new, 2), READY);
    
    printf("size cola new: %d \n", list_size(cola_new)); // 2 procesos
    printf("size cola ready: %d \n", list_size(cola_ready)); // 1 procesos
    printf("Cantidad total procesos: %d \n", list_size(cola_procesos)); // 3 procesos

    mostrar_pcb(*(t_pcb*)list_get(cola_ready, 0));

    //mostrar_pcb(*(t_pcb*)list_get(cola_running, 0));

    return EXIT_SUCCESS;
}

void iterator(char* value) {
    log_info(kernel_log, "%s", value);
}
