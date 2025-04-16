#include "../headers/kernel.h"

void* hilo_conexiones(void* _) {
    iniciar_conexiones_kernel();
    return NULL;
}

int main(int argc, char* argv[]) {
    iniciar_config_kernel();
    iniciar_logger_kernel();
    iniciar_estados_kernel();
    //pthread_t hilo_servidor;
    //pthread_create(&hilo_servidor, NULL, hilo_conexiones, NULL);

    INIT_PROC("Test1", 10);
    printf("size cola new: %d \n", list_size(cola_new));

    printf("Creando 2 procesos más... \n");
    INIT_PROC("Test2", 11);
    INIT_PROC("Test2", 12);
    printf("size cola new: %d \n", list_size(cola_new)); // 2 procesos
    printf("size cola ready: %d \n", list_size(cola_ready)); // 1 procesos
    printf("Cantidad total procesos: %d \n", list_size(cola_procesos)); // 3 procesos
    
    mostrar_pcb(*(t_pcb*)list_get(cola_new, 2));
    cambiar_estado_pcb((t_pcb*)list_get(cola_new, 2), BLOCKED);
    
    printf("size cola new: %d \n", list_size(cola_new)); // 2 procesos
    printf("size cola ready: %d \n", list_size(cola_ready)); // 1 procesos
    printf("Cantidad total procesos: %d \n", list_size(cola_procesos)); // 3 procesos

    mostrar_pcb(*(t_pcb*)list_get(cola_blocked, 0));

    //mostrar_pcb(*(t_pcb*)list_get(cola_running, 0));

    return EXIT_SUCCESS;
}

void iterator(char* value) {
    log_info(kernel_log, "%s", value);
}
