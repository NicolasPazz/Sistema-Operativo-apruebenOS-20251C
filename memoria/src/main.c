#include "../headers/main.h"

extern t_config_memoria* cfg;
extern t_log* logger;
extern t_list* segmentos_libres;

extern void* memoria_principal;
extern void* area_swap;

//extern sem_t SEM_COMPACTACION_DONE;
//extern sem_t SEM_COMPACTACION_START;

int main(int argc, char* argv[]) {

    iniciar_logger_memoria();
    if (!cargar_configuracion("memoria/memoria.config")) {
        log_error(logger, "Error al cargar la configuracion de memoria.");
        cerrar_programa();
        return EXIT_FAILURE;
    }

    // Inicializar las estructuras de memoria y las listas para instrucciones
    log_debug(logger, "Inicializando estructuras de memoria...");
    inicializar_memoria();
    inicializar_swap();
    instructions_init();
    memory_init();
    iniciar_mutex();

    log_debug(logger, "Memoria principal y estructuras inicializadas correctamente.");

    char* puerto = string_itoa(cfg->PUERTO_ESCUCHA);
    int memoria_server = iniciar_conexiones_memoria(puerto, logger);
    free(puerto);

    log_debug(logger, "Servidor de memoria iniciado correctamente. Esperando conexiones...");

    while (server_escuchar("Memoria", memoria_server));

    // Liberar recursos antes de salir
    instructions_destroy();
    memory_destroy();
    
    liberar_conexion(memoria_server);
    cerrar_programa();

    return EXIT_SUCCESS;
}

void iterator(char* value) {
    log_debug(logger, "%s", value);
}

