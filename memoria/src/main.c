#include "../headers/main.h"
#include <signal.h>

extern t_config_memoria* cfg;
extern t_log* logger;
extern t_sistema_memoria* sistema_memoria;

// Manejador de señales para terminación limpia
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n\nRecibida señal de terminación. Cerrando Memoria...\n");
        log_info(logger, "Recibida señal SIGINT. Iniciando terminación limpia de Memoria...");
        
        // Liberar recursos del sistema de memoria
        liberar_sistema_memoria();
        
        cerrar_programa();
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char* argv[]) {
    // Configurar el manejador de señales
    signal(SIGINT, signal_handler);

    // Inicializar logger
    iniciar_logger_memoria();
    if (!cargar_configuracion("memoria/memoria.config")) {
        log_error(logger, "Error al cargar la configuracion de memoria.");
        cerrar_programa();
        return EXIT_FAILURE;
    }

    log_info(logger, "=== INICIANDO SISTEMA DE MEMORIA ===");
    log_info(logger, "Configuración cargada - TAM_MEMORIA: %d, TAM_PAGINA: %d, NIVELES: %d, ENTRADAS_POR_TABLA: %d", 
             cfg->TAM_MEMORIA, cfg->TAM_PAGINA, cfg->CANTIDAD_NIVELES, cfg->ENTRADAS_POR_TABLA);

    // Inicializar el sistema completo de memoria con paginación multinivel
    if (inicializar_sistema_memoria() != MEMORIA_OK) {
        log_error(logger, "Error crítico al inicializar el sistema de memoria");
        cerrar_programa();
        return EXIT_FAILURE;
    }

    log_info(logger, "Sistema de memoria inicializado correctamente:");
    log_info(logger, "- Memoria principal: %d bytes (%d frames de %d bytes)", 
             cfg->TAM_MEMORIA, 
             sistema_memoria->admin_marcos->cantidad_total_frames, 
             cfg->TAM_PAGINA);
    log_info(logger, "- Sistema SWAP: %d bytes (%d páginas) en %s", 
             sistema_memoria->admin_swap->tamanio_swap,
             sistema_memoria->admin_swap->cantidad_paginas_swap,
             cfg->PATH_SWAPFILE);
    log_info(logger, "- Paginación: %d niveles con %d entradas por tabla", 
             cfg->CANTIDAD_NIVELES, cfg->ENTRADAS_POR_TABLA);

    log_info(logger, "Todas las estructuras inicializadas correctamente");

    // Iniciar servidor de memoria
    char* puerto = string_itoa(cfg->PUERTO_ESCUCHA);
    int memoria_server = iniciar_conexiones_memoria(puerto, logger);
    free(puerto);

    if (memoria_server == -1) {
        log_error(logger, "Error al iniciar el servidor de memoria");
        liberar_sistema_memoria();
        cerrar_programa();
        return EXIT_FAILURE;
    }

    log_info(logger, "=== SERVIDOR DE MEMORIA INICIADO ===");
    log_info(logger, "Escuchando en puerto %d. Esperando conexiones...", cfg->PUERTO_ESCUCHA);

  
    while (server_escuchar("Memoria", memoria_server));

    log_info(logger, "Finalizando servidor de memoria...");

    // Liberar recursos antes de salir
    liberar_sistema_memoria();
    
    liberar_conexion(memoria_server);
    cerrar_programa();

    return EXIT_SUCCESS;
}

void iterator(char* value) {
    log_debug(logger, "%s", value);
}

