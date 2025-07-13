#include "../headers/init_memoria.h"
#include "../headers/estructuras.h"
#include "../headers/manejo_memoria.h"
#include "../headers/metricas.h"
#include "../headers/manejo_swap.h"
#include "../headers/bloqueo_paginas.h"
#include <commons/log.h>
#include <commons/string.h>
#include <string.h>
#include <unistd.h>

// Variables globales
t_log* logger;
t_config_memoria* cfg;
t_sistema_memoria* sistema_memoria = NULL;

// Declaraciones de funciones internas
void liberar_instrucciones_dictionary(t_dictionary* dict);
void destruir_tabla_paginas_recursiva(t_tabla_paginas* tabla);

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN PRINCIPAL
// ============================================================================

int cargar_configuracion(char* path) {
    t_config* cfg_file = config_create(path);

    if (cfg_file == NULL) {
        printf("No se encontro el archivo de configuracion: %s\n", path);
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
        "PATH_INSTRUCCIONES",
        NULL
    };

    if (!config_has_all_properties(cfg_file, properties)) {
        printf("Propiedades faltantes en el archivo de configuracion\n");
        config_destroy(cfg_file);
        return 0;
    }

    cfg = malloc(sizeof(t_config_memoria));
    if (cfg == NULL) {
        printf("Error al asignar memoria para la configuracion\n");
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
    cfg->PATH_INSTRUCCIONES = strdup(config_get_string_value(cfg_file, "PATH_INSTRUCCIONES"));

    //printf("Archivo de configuracion cargado correctamente\n");
    config_destroy(cfg_file);

    return 1;
}

void iniciar_logger_memoria() {
    // Inicializar logger con configuración por defecto hasta cargar la configuración real
    logger = iniciar_logger("memoria/memoria.log", MODULENAME, 1, log_level_from_string(cfg->LOG_LEVEL));
    if (logger == NULL) {
        printf("Error al iniciar memoria logs\n");
    } else {
        log_trace(logger, "Memoria logs iniciados correctamente con configuracion temporal!");
    }
}

// ============================================================================
// IMPLEMENTACIÓN DEL ADMINISTRADOR CENTRALIZADO DE MARCOS FÍSICOS
// ============================================================================

t_administrador_marcos* crear_administrador_marcos(int cantidad_frames, int tam_pagina) {
    log_trace(logger, "## Inicializando administrador centralizado de marcos - Total: %d marcos", cantidad_frames);
    
    t_administrador_marcos* admin = malloc(sizeof(t_administrador_marcos));
    if (!admin) {
        log_error(logger, "Error al asignar memoria para administrador de marcos");
        return NULL;
    }
    
    // Inicializar configuración básica
    admin->cantidad_total_frames = cantidad_frames;
    admin->frames_libres = cantidad_frames;  // Inicialmente todos libres
    admin->frames_ocupados = 0;
    admin->total_asignaciones = 0;
    admin->total_liberaciones = 0;
    
    // Crear array de frames
    admin->frames = malloc(sizeof(t_frame) * cantidad_frames);
    if (!admin->frames) {
        log_error(logger, "Error al asignar memoria para array de frames");
        free(admin);
        return NULL;
    }
    
    // Inicializar cada frame
    for (int i = 0; i < cantidad_frames; i++) {
        admin->frames[i].numero_frame = i;
        admin->frames[i].ocupado = false;
        admin->frames[i].pid_propietario = -1;
        admin->frames[i].numero_pagina = -1;
        admin->frames[i].contenido = (char*)sistema_memoria->memoria_principal + (i * tam_pagina);
        admin->frames[i].timestamp_asignacion = 0;
    }
    
    // Crear bitmap para búsqueda eficiente
    size_t bitmap_size = (cantidad_frames + 7) / 8;  // Redondear hacia arriba
    char* bitmap_data = calloc(bitmap_size, 1);  // Inicializar en 0 (todos libres)
    if (!bitmap_data) {
        log_error(logger, "Error al crear bitmap de frames");
        free(admin->frames);
        free(admin);
        return NULL;
    }
    
    admin->bitmap_frames = bitarray_create_with_mode(bitmap_data, bitmap_size, MSB_FIRST);
    if (!admin->bitmap_frames) {
        log_error(logger, "Error al inicializar bitarray");
        free(bitmap_data);
        free(admin->frames);
        free(admin);
        return NULL;
    }
    
    // Crear lista de frames libres para acceso O(1)
    admin->lista_frames_libres = list_create();
    if (!admin->lista_frames_libres) {
        log_error(logger, "Error al crear lista de frames libres");
        bitarray_destroy(admin->bitmap_frames);
        free(admin->frames);
        free(admin);
        return NULL;
    }
    
    // Agregar todos los frames a la lista de libres
    for (int i = 0; i < cantidad_frames; i++) {
        int* frame_num = malloc(sizeof(int));
        *frame_num = i;
        list_add(admin->lista_frames_libres, frame_num);
    }
    
    // Inicializar mutex
    if (pthread_mutex_init(&admin->mutex_frames, NULL) != 0) {
        log_error(logger, "Error al inicializar mutex de frames");
        list_destroy_and_destroy_elements(admin->lista_frames_libres, free);
        bitarray_destroy(admin->bitmap_frames);
        free(admin->frames);
        free(admin);
        return NULL;
    }
    
    log_trace(logger, "## Administrador de marcos inicializado correctamente - %d frames disponibles", cantidad_frames);
    return admin;
}

void destruir_administrador_marcos(t_administrador_marcos* admin) {
    if (!admin) return;
    
    log_trace(logger, "## Destruyendo administrador de marcos - Estadísticas finales:");
    log_trace(logger, "   - Total asignaciones: %d", admin->total_asignaciones);
    log_trace(logger, "   - Total liberaciones: %d", admin->total_liberaciones);
    log_trace(logger, "   - Frames libres al finalizar: %d", admin->frames_libres);
    
    // Destruir mutex
    pthread_mutex_destroy(&admin->mutex_frames);
    
    // Liberar lista de frames libres
    if (admin->lista_frames_libres) {
        list_destroy_and_destroy_elements(admin->lista_frames_libres, free);
    }
    
    // Liberar bitmap
    if (admin->bitmap_frames) {
        bitarray_destroy(admin->bitmap_frames);
    }
    
    // Liberar array de frames
    if (admin->frames) {
        free(admin->frames);
    }
    
    free(admin);
}

int asignar_marco_libre(int pid, int numero_pagina) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        log_error(logger, "Sistema de memoria no inicializado");
        return -1;
    }
    
    t_administrador_marcos* admin = sistema_memoria->admin_marcos;
    
    pthread_mutex_lock(&admin->mutex_frames);
    
    // Verificar si hay marcos disponibles
    if (admin->frames_libres == 0 || list_is_empty(admin->lista_frames_libres)) {
        log_warning(logger, "## No hay marcos libres disponibles - PID: %d, Página: %d", pid, numero_pagina);
        pthread_mutex_unlock(&admin->mutex_frames);
        return -1;
    }
    
    // Obtener el primer marco libre
    int* frame_num_ptr = (int*)list_remove(admin->lista_frames_libres, 0);
    int numero_frame = *frame_num_ptr;
    free(frame_num_ptr);
    
    // Configurar el frame
    t_frame* frame = &admin->frames[numero_frame];
    frame->ocupado = true;
    frame->pid_propietario = pid;
    frame->numero_pagina = numero_pagina;
    frame->timestamp_asignacion = time(NULL);
    
    // Actualizar bitmap (marcar como ocupado)
    bitarray_set_bit(admin->bitmap_frames, numero_frame);
    
    // Actualizar contadores
    admin->frames_libres--;
    admin->frames_ocupados++;
    admin->total_asignaciones++;
    
    pthread_mutex_unlock(&admin->mutex_frames);
    
    log_trace(logger, "## Marco asignado - Frame: %d, PID: %d, Página: %d", numero_frame, pid, numero_pagina);
    log_trace(logger, "[MARCOS] Total de marcos libres tras asignar: %d", admin->frames_libres);
    return numero_frame;
}

t_resultado_memoria liberar_marco(int numero_frame) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        log_error(logger, "Sistema de memoria no inicializado");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    t_administrador_marcos* admin = sistema_memoria->admin_marcos;
    
    if (numero_frame < 0 || numero_frame >= admin->cantidad_total_frames) {
        log_error(logger, "Número de frame inválido: %d", numero_frame);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    pthread_mutex_lock(&admin->mutex_frames);
    
    t_frame* frame = &admin->frames[numero_frame];
    
    if (!frame->ocupado) {
        log_warning(logger, "Intento de liberar frame ya libre: %d (FORZANDO LIBERACIÓN DE TODAS FORMAS)", numero_frame);
        // No return: continuar y limpiar igual
    }
    
    int pid_anterior = frame->pid_propietario;
    int pagina_anterior = frame->numero_pagina;
    
    // Limpiar el frame SIEMPRE
    frame->ocupado = false;
    frame->pid_propietario = -1;
    frame->numero_pagina = -1;
    frame->timestamp_asignacion = 0;
    
    // Limpiar contenido del frame (opcional, para debugging)
    memset(frame->contenido, 0, sistema_memoria->tam_pagina);
    
    // Actualizar bitmap (marcar como libre)
    bitarray_clean_bit(admin->bitmap_frames, numero_frame);
    
    // Agregar a lista de frames libres SOLO si no estaba ya
    bool ya_en_lista = false;
    for (int i = 0; i < list_size(admin->lista_frames_libres); i++) {
        int* f = list_get(admin->lista_frames_libres, i);
        if (*f == numero_frame) {
            ya_en_lista = true;
            break;
        }
    }
    if (!ya_en_lista) {
    int* frame_num = malloc(sizeof(int));
    *frame_num = numero_frame;
    list_add(admin->lista_frames_libres, frame_num);
    }
    
    // Actualizar contadores SIEMPRE
    admin->frames_libres++;
    admin->frames_ocupados--;
    admin->total_liberaciones++;
    
    pthread_mutex_unlock(&admin->mutex_frames);
    
    log_trace(logger, "## Marco liberado - Frame: %d (era PID: %d, Página: %d)", numero_frame, pid_anterior, pagina_anterior);
    log_trace(logger, "[MARCOS] Total de marcos libres tras liberar: %d", admin->frames_libres);
    return MEMORIA_OK;
}

t_frame* obtener_frame(int numero_frame) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        return NULL;
    }
    
    t_administrador_marcos* admin = sistema_memoria->admin_marcos;
    
    if (numero_frame < 0 || numero_frame >= admin->cantidad_total_frames) {
        return NULL;
    }
    
    return &admin->frames[numero_frame];
}

int obtener_marcos_libres(void) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        return 0;
    }
    
    return sistema_memoria->admin_marcos->frames_libres;
}

void obtener_estadisticas_marcos(int* total_frames, int* frames_libres, int* frames_ocupados) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        if (total_frames) *total_frames = 0;
        if (frames_libres) *frames_libres = 0;
        if (frames_ocupados) *frames_ocupados = 0;
        return;
    }
    
    t_administrador_marcos* admin = sistema_memoria->admin_marcos;
    
    pthread_mutex_lock(&admin->mutex_frames);
    
    if (total_frames) *total_frames = admin->cantidad_total_frames;
    if (frames_libres) *frames_libres = admin->frames_libres;
    if (frames_ocupados) *frames_ocupados = admin->frames_ocupados;
    
    pthread_mutex_unlock(&admin->mutex_frames);
}

// ============================================================================
// ADMINISTRADOR DE SWAP
// ============================================================================

t_administrador_swap* crear_administrador_swap(void) {
    log_trace(logger, "## Inicializando administrador de SWAP");
    
    t_administrador_swap* admin = malloc(sizeof(t_administrador_swap));
    if (!admin) {
        log_error(logger, "Error al asignar memoria para administrador de SWAP");
        return NULL;
    }
    
    // Configuración básica
    admin->path_archivo = strdup(cfg->PATH_SWAPFILE);
    admin->tam_pagina = cfg->TAM_PAGINA;
    admin->tamanio_swap = cfg->TAM_MEMORIA * 10;  // Tamaño de SWAP
    admin->cantidad_paginas_swap = admin->tamanio_swap / admin->tam_pagina;
    admin->paginas_libres_swap = admin->cantidad_paginas_swap;
    admin->paginas_ocupadas_swap = 0;
    admin->total_escrituras_swap = 0;
    admin->total_lecturas_swap = 0;
    
    // Crear archivo de SWAP
    admin->fd_swap = open(admin->path_archivo, O_CREAT | O_RDWR, 0644);
    if (admin->fd_swap == -1) {
        log_error(logger, "Error al crear archivo de SWAP: %s", admin->path_archivo);
        free(admin->path_archivo);
        free(admin);
        return NULL;
    }
    
    // Extender archivo al tamaño necesario
    if (ftruncate(admin->fd_swap, admin->tamanio_swap) == -1) {
        log_error(logger, "Error al dimensionar archivo de SWAP");
        close(admin->fd_swap);
        free(admin->path_archivo);
        free(admin);
        return NULL;
    }
    
    // Crear array de entradas
    admin->entradas = malloc(sizeof(t_entrada_swap) * admin->cantidad_paginas_swap);
    if (!admin->entradas) {
        log_error(logger, "Error al crear entradas de SWAP");
        close(admin->fd_swap);
        free(admin->path_archivo);
        free(admin);
        return NULL;
    }
    
    // Inicializar entradas
    for (int i = 0; i < admin->cantidad_paginas_swap; i++) {
        admin->entradas[i].ocupado = false;
        admin->entradas[i].pid_propietario = -1;
        admin->entradas[i].numero_pagina = -1;
        admin->entradas[i].offset_archivo = i * admin->tam_pagina;
        admin->entradas[i].timestamp_escritura = 0;
    }
    
    // Crear lista de posiciones libres
    admin->posiciones_libres = list_create();
    for (int i = 0; i < admin->cantidad_paginas_swap; i++) {
        int* pos = malloc(sizeof(int));
        *pos = i;
        list_add(admin->posiciones_libres, pos);
    }
    
    // Inicializar mutex
    if (pthread_mutex_init(&admin->mutex_swap, NULL) != 0) {
        log_error(logger, "Error al inicializar mutex de SWAP");
        list_destroy_and_destroy_elements(admin->posiciones_libres, free);
        free(admin->entradas);
        close(admin->fd_swap);
        free(admin->path_archivo);
        free(admin);
        return NULL;
    }
    
    log_trace(logger, "## Administrador de SWAP inicializado - %d páginas disponibles", admin->cantidad_paginas_swap);
    return admin;
}

void destruir_administrador_swap(t_administrador_swap* admin) {
    if (!admin) return;
    
    log_trace(logger, "## Destruyendo administrador de SWAP - Estadísticas finales:");
    log_trace(logger, "   - Total escrituras: %d", admin->total_escrituras_swap);
    log_trace(logger, "   - Total lecturas: %d", admin->total_lecturas_swap);
    log_trace(logger, "   - Páginas libres al finalizar: %d", admin->paginas_libres_swap);
    
    pthread_mutex_destroy(&admin->mutex_swap);
    
    if (admin->posiciones_libres) {
        list_destroy_and_destroy_elements(admin->posiciones_libres, free);
    }
    
    if (admin->entradas) {
        free(admin->entradas);
    }
    
    if (admin->fd_swap != -1) {
        close(admin->fd_swap);
    }
    
    if (admin->path_archivo) {
        free(admin->path_archivo);
    }
    
    free(admin);
}

// ============================================================================
// FUNCIONES REFACTORIZADAS DE INICIALIZACIÓN
// ============================================================================

t_resultado_memoria inicializar_sistema_memoria(void) {
    log_trace(logger, "## Inicializando sistema completo de memoria");
    
    // Crear estructura principal del sistema
    sistema_memoria = malloc(sizeof(t_sistema_memoria));
    if (!sistema_memoria) {
        log_error(logger, "Error al asignar memoria para sistema_memoria");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Inicializar configuración desde archivo
    sistema_memoria->tam_memoria = cfg->TAM_MEMORIA;
    sistema_memoria->tam_pagina = cfg->TAM_PAGINA;
    sistema_memoria->entradas_por_tabla = cfg->ENTRADAS_POR_TABLA;
    sistema_memoria->cantidad_niveles = cfg->CANTIDAD_NIVELES;
    sistema_memoria->retardo_memoria = cfg->RETARDO_MEMORIA;
    sistema_memoria->retardo_swap = cfg->RETARDO_SWAP;
    
    // Inicializar estadísticas
    sistema_memoria->procesos_activos = 0;
    sistema_memoria->procesos_suspendidos = 0;
    sistema_memoria->memoria_utilizada = 0;
    sistema_memoria->swap_utilizado = 0;
    sistema_memoria->total_asignaciones_memoria = 0;
    sistema_memoria->total_liberaciones_memoria = 0;
    sistema_memoria->total_suspensiones = 0;
    sistema_memoria->total_reanudaciones = 0;
    
    // Crear memoria física principal
    sistema_memoria->memoria_principal = malloc(cfg->TAM_MEMORIA);
    if (!sistema_memoria->memoria_principal) {
        log_error(logger, "Error al asignar memoria principal de %d bytes", cfg->TAM_MEMORIA);
        free(sistema_memoria);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Inicializar memoria con ceros
    memset(sistema_memoria->memoria_principal, 0, cfg->TAM_MEMORIA);
    log_trace(logger, "## Memoria principal inicializada - %d bytes", cfg->TAM_MEMORIA);
    
    // Crear administrador centralizado de marcos
    int cantidad_frames = cfg->TAM_MEMORIA / cfg->TAM_PAGINA;
    sistema_memoria->admin_marcos = crear_administrador_marcos(cantidad_frames, cfg->TAM_PAGINA);
    if (!sistema_memoria->admin_marcos) {
        log_error(logger, "Error al crear administrador de marcos");
        free(sistema_memoria->memoria_principal);
        free(sistema_memoria);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Crear diccionarios principales
    sistema_memoria->procesos = dictionary_create();
    sistema_memoria->estructuras_paginas = dictionary_create();
    sistema_memoria->metricas_procesos = dictionary_create();
    sistema_memoria->process_instructions = dictionary_create();
    
    if (!sistema_memoria->procesos || !sistema_memoria->estructuras_paginas || 
        !sistema_memoria->metricas_procesos || !sistema_memoria->process_instructions) {
        log_error(logger, "Error al crear diccionarios del sistema");
        finalizar_sistema_memoria();
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Inicializar mutex del sistema
    if (pthread_mutex_init(&sistema_memoria->mutex_sistema, NULL) != 0 ||
        pthread_mutex_init(&sistema_memoria->mutex_procesos, NULL) != 0 ||
        pthread_mutex_init(&sistema_memoria->mutex_estadisticas, NULL) != 0) {
        log_error(logger, "Error al inicializar mutex del sistema");
        finalizar_sistema_memoria();
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Inicializar administrador de SWAP
    sistema_memoria->admin_swap = crear_administrador_swap();
    if (!sistema_memoria->admin_swap) {
        log_error(logger, "Error al crear administrador de SWAP");
        finalizar_sistema_memoria();
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    log_trace(logger, "## Sistema de memoria inicializado correctamente");
    log_trace(logger, "   - Memoria física: %d bytes (%d marcos de %d bytes)", 
             cfg->TAM_MEMORIA, cantidad_frames, cfg->TAM_PAGINA);
    log_trace(logger, "   - Paginación: %d niveles, %d entradas por tabla", 
             cfg->CANTIDAD_NIVELES, cfg->ENTRADAS_POR_TABLA);
    log_trace(logger, "   - Retardos: Memoria %dms, SWAP %dms", 
             cfg->RETARDO_MEMORIA, cfg->RETARDO_SWAP);
    
    return MEMORIA_OK;
}

void finalizar_sistema_memoria(void) {
    if (!sistema_memoria) {
        return;
    }

    // Liberar memoria principal
    if (sistema_memoria->memoria_principal) {
        free(sistema_memoria->memoria_principal);
    }

    // Liberar administrador de marcos
    if (sistema_memoria->admin_marcos) {
        destruir_administrador_marcos(sistema_memoria->admin_marcos);
    }

    // Liberar administrador de SWAP
    if (sistema_memoria->admin_swap) {
        destruir_administrador_swap(sistema_memoria->admin_swap);
    }

    // Liberar diccionarios
    if (sistema_memoria->procesos) {
        dictionary_destroy_and_destroy_elements(sistema_memoria->procesos, (void*)destruir_proceso);
    }
    if (sistema_memoria->estructuras_paginas) {
        dictionary_destroy(sistema_memoria->estructuras_paginas); // NO destruir elementos
    }
    if (sistema_memoria->metricas_procesos) {
        dictionary_destroy(sistema_memoria->metricas_procesos); // NO destruir elementos
    }
    if (sistema_memoria->process_instructions) {
        liberar_instrucciones_dictionary(sistema_memoria->process_instructions);
    }

    // Destruir mutexes
    pthread_mutex_destroy(&sistema_memoria->mutex_sistema);
    pthread_mutex_destroy(&sistema_memoria->mutex_procesos);
    pthread_mutex_destroy(&sistema_memoria->mutex_estadisticas);

    // Liberar estructura principal
    free(sistema_memoria);
    sistema_memoria = NULL;
}

void liberar_sistema_memoria(void) {
    finalizar_sistema_memoria();
}

// ============================================================================
// FUNCIONES DE UTILIDAD
// ============================================================================

void cerrar_programa() {
    log_trace(logger, "## Cerrando programa de memoria");
    
    finalizar_sistema_memoria();
    
    if (cfg) {
        if (cfg->PATH_SWAPFILE) free(cfg->PATH_SWAPFILE);
        if (cfg->LOG_LEVEL) free(cfg->LOG_LEVEL);
        if (cfg->DUMP_PATH) free(cfg->DUMP_PATH);
        if (cfg->PATH_INSTRUCCIONES) free(cfg->PATH_INSTRUCCIONES);
        free(cfg);
    }
    
    if (logger) {
        log_trace(logger, "## Programa de memoria finalizado correctamente");

        log_destroy(logger);
    }
    
}

// ============================================================================
// FUNCIONES DE DESTRUCCIÓN DE ESTRUCTURAS
// ============================================================================

void destruir_estructura_paginas(t_estructura_paginas* estructura) {
    if (!estructura) return;
    
    // Destruir tabla de páginas recursivamente
    destruir_tabla_paginas_recursiva(estructura->tabla_raiz);
    
    // Liberar estructura
    free(estructura);
}

void destruir_metricas_proceso(t_metricas_proceso* metricas) {
    if (!metricas) return;
    free(metricas);
}

void destruir_tabla_paginas_recursiva(t_tabla_paginas* tabla) {
    if (!tabla) return;
    
    // Si no es el último nivel, destruir recursivamente las tablas hijas
    if (tabla->nivel > 0) {
        for (int i = 0; i < cfg->ENTRADAS_POR_TABLA; i++) {
            if (tabla->entradas[i].presente && tabla->entradas[i].tabla_siguiente) {
                destruir_tabla_paginas_recursiva(tabla->entradas[i].tabla_siguiente);
            }
        }
    }

    // Liberar memoria de las entradas y la tabla
    free(tabla->entradas);
    free(tabla);
}

void _liberar_instrucciones(char* key, void* value) {
    t_list* instrucciones = (t_list*)value;
    if (instrucciones) {
        list_destroy_and_destroy_elements(instrucciones, free);
    }
}

void liberar_instrucciones_dictionary(t_dictionary* dict) {
    if (!dict) return;
    dictionary_iterator(dict, _liberar_instrucciones);
    dictionary_destroy(dict);
}