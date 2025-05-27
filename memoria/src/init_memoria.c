#include "../headers/init_memoria.h"

// varios
t_log* logger;
t_config_memoria* cfg;
//bool seg;

// segmentacion
t_list* segmentos_libres;
t_list* segmentos_usados;
int memoria_disponible;
//segmento_t* (*proximo_hueco)(int);

// paginacion
//frame_t* tabla_frames;
//frame_swap_t* tabla_frames_swap;
t_list* tid_pid_lookup;
int espacio_disponible_swap;
int global_TUR; // evil

void* area_swap;

void* memoria_principal;


void iniciar_logger_memoria() {
    logger = iniciar_logger("memoria.log", MODULENAME, 1, LOG_LEVEL_TRACE);  // Log level deberia venir del config, no hardcodeado
    if (logger == NULL) {
        printf("Error al iniciar memoria logs\n");
    } else {
        log_debug(logger, "Memoria logs iniciados correctamente!");
    }
}

uint8_t cargar_configuracion(char* path) {
    t_config* cfg_file = config_create(path);

    if (cfg_file == NULL) {
        log_error(logger, "No se encontro el archivo de configuracion: %s", path);
        return 0;
    }

    char* properties[] = {
        "PUERTO_ESCUCHA",
        "TAM_MEMORIA",
        "TAM_PAGINA",
        "ENTRADAS_POR_TABLA",
        "CANTIDAD_NIVELES",
        "RETARDO_MEMORIA",
        "PATH_SWAPFILE",
        "RETARDO_SWAP",
        "LOG_LEVEL",
        "DUMP_PATH",
        NULL
    };

    if (!config_has_all_properties(cfg_file, properties)) {
        log_error(logger, "Propiedades faltantes en el archivo de configuracion");
        config_destroy(cfg_file);
        return 0;
    }

    cfg = malloc(sizeof(t_config_memoria));
    if (cfg == NULL) {
        log_error(logger, "Error al asignar memoria para la configuracion");
        config_destroy(cfg_file);
        return 0;
    }

    cfg->PUERTO_ESCUCHA = config_get_int_value(cfg_file, "PUERTO_ESCUCHA");
    cfg->TAM_MEMORIA = config_get_int_value(cfg_file, "TAM_MEMORIA");
    cfg->TAM_PAGINA = config_get_int_value(cfg_file, "TAM_PAGINA");
    cfg->ENTRADAS_POR_TABLA = config_get_int_value(cfg_file, "ENTRADAS_POR_TABLA");
    cfg->CANTIDAD_NIVELES = config_get_int_value(cfg_file, "CANTIDAD_NIVELES");
    cfg->RETARDO_MEMORIA = config_get_int_value(cfg_file, "RETARDO_MEMORIA");
    cfg->PATH_SWAPFILE = strdup(config_get_string_value(cfg_file, "PATH_SWAPFILE"));
    cfg->RETARDO_SWAP = config_get_int_value(cfg_file, "RETARDO_SWAP");
    cfg->LOG_LEVEL = strdup(config_get_string_value(cfg_file, "LOG_LEVEL"));
    cfg->DUMP_PATH = strdup(config_get_string_value(cfg_file, "DUMP_PATH"));

    log_debug(logger, "Archivo de configuracion cargado correctamente");
    config_destroy(cfg_file);

    return 1;
}



void cerrar_programa() {
    log_debug(logger, "Finalizando programa...");

    // Liberar mutex y semáforos
    finalizar_mutex();
    
    // Liberar memoria principal
    if (memoria_principal != NULL) {
        free(memoria_principal);
        memoria_principal = NULL;
        log_debug(logger, "Memoria principal liberada correctamente");
    }
    
    // Desasignar área de swap
    if (area_swap != NULL && area_swap != MAP_FAILED) {
        // Obtener el tamaño de swap
        int swap_size = cfg ? cfg->TAM_MEMORIA * 2 : 0;
        if (swap_size > 0) {
            munmap(area_swap, swap_size);
            log_debug(logger, "Área de swap liberada correctamente");
        }
        area_swap = NULL;
    }
    
    // Borrar archivo swap si existe
    if (cfg && cfg->PATH_SWAPFILE) {
        unlink(cfg->PATH_SWAPFILE);
        log_debug(logger, "Archivo SWAP eliminado correctamente");
    }

    if (cfg != NULL) {
        if (cfg->PATH_SWAPFILE) free(cfg->PATH_SWAPFILE);
        if (cfg->LOG_LEVEL) free(cfg->LOG_LEVEL);
        if (cfg->DUMP_PATH) free(cfg->DUMP_PATH);
        free(cfg);
        cfg = NULL;
    }

    if (logger != NULL) {
        log_destroy(logger);
        logger = NULL;
    }
}