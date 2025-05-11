#include "../headers/main.h"

extern t_config_memoria* cfg;
extern t_log* memoria_log;
extern t_list* segmentos_libres;

extern void* memoria_principal;
extern void* area_swap;

//extern sem_t SEM_COMPACTACION_DONE;
//extern sem_t SEM_COMPACTACION_START;

int main(int argc, char* argv[]) {

    iniciar_logger_memoria();

    if (!cargar_configuracion("memoria.config")) {
        log_error(memoria_log, "Error al cargar la configuracion de memoria.");
        cerrar_programa();
        return EXIT_FAILURE;
    }

    char* puerto = string_itoa(cfg->PUERTO_ESCUCHA);
    //int memoria_server = iniciar_servidor(puerto, memoria_log, "Servidor de memoria");
    //log_debug(memoria_log, "Antes de iniciar conexiones de memoria");
    int memoria_server = iniciar_conexiones_memoria(puerto, memoria_log);
    //log_debug(memoria_log, "Despues de iniciar conexiones de memoria");
    free(puerto);

    /* if (memoria_server == -1) {
        log_error(memoria_log, "No se pudo iniciar el servidor de memoria.");
        cerrar_programa();
        return EXIT_FAILURE;
    } */

    log_debug(memoria_log, "Servidor de memoria iniciado correctamente. Esperando conexiones...");

    while (server_escuchar("Memoria", memoria_server));

    liberar_conexion(memoria_server);
    cerrar_programa();

    return EXIT_SUCCESS;
}

void iterator(char* value) {
    log_debug(memoria_log, "%s", value);
}

