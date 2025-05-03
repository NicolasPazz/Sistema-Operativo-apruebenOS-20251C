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

    if (!cargar_configuracion("memoria.config")) {
        log_error(logger, "Error al cargar la configuración de memoria.");
        cerrar_programa();
        return EXIT_FAILURE;
    }

    char* puerto = string_itoa(cfg->PUERTO_ESCUCHA);
    //int memoria_server = iniciar_servidor(puerto, logger, "Servidor de memoria");
    //log_info(logger, "Antes de iniciar conexiones de memoria");
    int memoria_server = iniciar_conexiones_memoria(puerto, logger);
    //log_info(logger, "Después de iniciar conexiones de memoria");
    free(puerto);

    /* if (memoria_server == -1) {
        log_error(logger, "No se pudo iniciar el servidor de memoria.");
        cerrar_programa();
        return EXIT_FAILURE;
    } */

    log_info(logger, "Servidor de memoria iniciado correctamente. Esperando conexiones...");

    while (server_escuchar("Memoria", memoria_server));

    liberar_conexion(memoria_server);
    cerrar_programa();

    return EXIT_SUCCESS;
}

void iterator(char* value) {
    log_info(logger, "%s", value);
}

