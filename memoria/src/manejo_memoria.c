#include "../headers/manejo_memoria.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include <commons/log.h>
#include <commons/string.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Variables externas
extern t_sistema_memoria* sistema_memoria;
extern t_log* logger;
extern t_config_memoria* cfg;

// ============================================================================
// FUNCIONES DE GESTIÓN DE PROCESOS EN MEMORIA
// ============================================================================

t_resultado_memoria crear_proceso_en_memoria(int pid, int tamanio) {
    log_info(logger, "## PID: %d - Proceso Creado - Tamaño: %d", pid, tamanio);
    
    if (!sistema_memoria) {
        log_error(logger, "Sistema de memoria no inicializado");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Verificar si el proceso ya existe
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&sistema_memoria->mutex_procesos);
    
    if (dictionary_has_key(sistema_memoria->procesos, pid_str)) {
        log_error(logger, "PID: %d - Proceso ya existe", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_PROCESO_EXISTENTE;
    }
    
    // Calcular páginas necesarias
    int paginas_necesarias = (tamanio + cfg->TAM_PAGINA - 1) / cfg->TAM_PAGINA;
    
    // Verificar si hay suficientes marcos libres
    if (obtener_marcos_libres() < paginas_necesarias) {
        log_error(logger, "PID: %d - Memoria insuficiente para crear proceso. Necesarias: %d, Disponibles: %d", 
                  pid, paginas_necesarias, obtener_marcos_libres());
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Crear proceso en memoria
    t_proceso_memoria* proceso = malloc(sizeof(t_proceso_memoria));
    if (!proceso) {
        log_error(logger, "PID: %d - Error al asignar memoria para proceso", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Inicializar proceso
    proceso->pid = pid;
    proceso->tamanio = tamanio;
    proceso->activo = true;
    proceso->suspendido = false;
    proceso->timestamp_creacion = time(NULL);
    
    // Crear estructura de paginación multinivel
    proceso->estructura_paginas = crear_estructura_paginas(pid, tamanio);
    if (!proceso->estructura_paginas) {
        log_error(logger, "PID: %d - Error al crear estructura de paginación", pid);
        free(proceso);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Crear métricas del proceso
    proceso->metricas = crear_metricas_proceso(pid);
    if (!proceso->metricas) {
        log_error(logger, "PID: %d - Error al crear métricas", pid);
        destruir_estructura_paginas(proceso->estructura_paginas);
        free(proceso);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Crear lista de instrucciones
    proceso->instrucciones = list_create();
    if (!proceso->instrucciones) {
        log_error(logger, "PID: %d - Error al crear lista de instrucciones", pid);
        destruir_metricas_proceso(proceso->metricas);
        destruir_estructura_paginas(proceso->estructura_paginas);
        free(proceso);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Asignar marcos físicos para las páginas del proceso
    for (int i = 0; i < paginas_necesarias; i++) {
        int numero_frame = asignar_marco_libre(pid, i);
        if (numero_frame == -1) {
            log_error(logger, "PID: %d - Error al asignar marco para página %d", pid, i);
            // Liberar marcos ya asignados
            for (int j = 0; j < i; j++) {
                // Buscar y liberar marcos del proceso
                liberar_marcos_proceso(pid);
            }
            list_destroy(proceso->instrucciones);
            destruir_metricas_proceso(proceso->metricas);
            destruir_estructura_paginas(proceso->estructura_paginas);
            free(proceso);
            pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
            return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
        }
        
        // Configurar entrada en la tabla de páginas
        configurar_entrada_pagina(proceso->estructura_paginas, i, numero_frame);
    }
    
    // Agregar proceso al diccionario
    dictionary_put(sistema_memoria->procesos, pid_str, proceso);
    dictionary_put(sistema_memoria->estructuras_paginas, pid_str, proceso->estructura_paginas);
    dictionary_put(sistema_memoria->metricas_procesos, pid_str, proceso->metricas);
    dictionary_put(sistema_memoria->process_instructions, pid_str, proceso->instrucciones);
    
    // Actualizar estadísticas del sistema
    sistema_memoria->procesos_activos++;
    sistema_memoria->memoria_utilizada += tamanio;
    sistema_memoria->total_asignaciones_memoria++;
    
    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
    
    log_info(logger, "## PID: %d - Proceso creado exitosamente - %d páginas asignadas", pid, paginas_necesarias);
    return MEMORIA_OK;
}

t_resultado_memoria finalizar_proceso_en_memoria(int pid) {
    if (!sistema_memoria) {
        log_error(logger, "Sistema de memoria no inicializado");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&sistema_memoria->mutex_procesos);
    
    // Verificar si el proceso existe
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    if (!proceso) {
        log_error(logger, "PID: %d - Proceso no encontrado para finalizar", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    // Imprimir métricas antes de finalizar
    imprimir_metricas_proceso(proceso->metricas);
    
    // Liberar todos los marcos físicos del proceso
    liberar_marcos_proceso(pid);
    
    // Liberar estructuras del proceso
    if (proceso->instrucciones) {
        list_destroy_and_destroy_elements(proceso->instrucciones, free);
    }
    
    if (proceso->estructura_paginas) {
        destruir_estructura_paginas(proceso->estructura_paginas);
    }
    
    if (proceso->metricas) {
        destruir_metricas_proceso(proceso->metricas);
    }
    
    // Actualizar estadísticas del sistema
    sistema_memoria->procesos_activos--;
    sistema_memoria->memoria_utilizada -= proceso->tamanio;
    sistema_memoria->total_liberaciones_memoria++;
    
    // Remover del diccionario y liberar
    dictionary_remove(sistema_memoria->procesos, pid_str);
    dictionary_remove(sistema_memoria->estructuras_paginas, pid_str);
    dictionary_remove(sistema_memoria->metricas_procesos, pid_str);
    dictionary_remove(sistema_memoria->process_instructions, pid_str);
    
    free(proceso);
    
    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
    
    log_info(logger, "## PID: %d - Finaliza el proceso", pid);
    return MEMORIA_OK;
}

t_resultado_memoria suspender_proceso_en_memoria(int pid) {
    if (!sistema_memoria) {
        log_error(logger, "Sistema de memoria no inicializado");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&sistema_memoria->mutex_procesos);
    
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    if (!proceso) {
        log_error(logger, "PID: %d - Proceso no encontrado para suspender", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    if (proceso->suspendido) {
        log_warning(logger, "PID: %d - Proceso ya está suspendido", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_OK;
    }
    
    // Escribir páginas del proceso a SWAP
    t_resultado_memoria resultado = escribir_proceso_a_swap(pid);
    if (resultado != MEMORIA_OK) {
        log_error(logger, "PID: %d - Error al escribir proceso a SWAP", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return resultado;
    }
    
    // Liberar marcos físicos del proceso
    liberar_marcos_proceso(pid);
    
    // Marcar proceso como suspendido
    proceso->suspendido = true;
    proceso->activo = false;
    
    // Actualizar estadísticas
    sistema_memoria->procesos_activos--;
    sistema_memoria->procesos_suspendidos++;
    sistema_memoria->memoria_utilizada -= proceso->tamanio;
    sistema_memoria->total_suspensiones++;
    
    // Incrementar métrica de bajadas a SWAP
    incrementar_bajadas_swap(pid);
    
    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
    
    log_info(logger, "## PID: %d - Proceso suspendido exitosamente", pid);
    return MEMORIA_OK;
}

t_resultado_memoria reanudar_proceso_en_memoria(int pid) {
    if (!sistema_memoria) {
        log_error(logger, "Sistema de memoria no inicializado");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&sistema_memoria->mutex_procesos);
    
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    if (!proceso) {
        log_error(logger, "PID: %d - Proceso no encontrado para reanudar", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    if (!proceso->suspendido) {
        log_warning(logger, "PID: %d - Proceso no está suspendido", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_OK;
    }
    
    // Verificar si hay suficientes marcos libres
    int paginas_necesarias = proceso->estructura_paginas->paginas_totales;
    if (obtener_marcos_libres() < paginas_necesarias) {
        log_error(logger, "PID: %d - Memoria insuficiente para reanudar. Necesarias: %d, Disponibles: %d", 
                  pid, paginas_necesarias, obtener_marcos_libres());
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Leer proceso desde SWAP
    t_resultado_memoria resultado = leer_proceso_desde_swap(pid);
    if (resultado != MEMORIA_OK) {
        log_error(logger, "PID: %d - Error al leer proceso desde SWAP", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return resultado;
    }
    
    // Asignar nuevos marcos para todas las páginas
    for (int i = 0; i < paginas_necesarias; i++) {
        int numero_frame = asignar_marco_libre(pid, i);
        if (numero_frame == -1) {
            log_error(logger, "PID: %d - Error al asignar marco para página %d", pid, i);
            pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
            return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
        }
        
        // Actualizar entrada en tabla de páginas
        configurar_entrada_pagina(proceso->estructura_paginas, i, numero_frame);
    }
    
    // Marcar proceso como activo
    proceso->suspendido = false;
    proceso->activo = true;
    
    // Actualizar estadísticas
    sistema_memoria->procesos_activos++;
    sistema_memoria->procesos_suspendidos--;
    sistema_memoria->memoria_utilizada += proceso->tamanio;
    sistema_memoria->total_reanudaciones++;
    
    // Incrementar métrica de subidas a memoria principal
    incrementar_subidas_memoria_principal(pid);
    
    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
    
    log_info(logger, "## PID: %d - Proceso reanudado exitosamente", pid);
    return MEMORIA_OK;
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE ESTRUCTURAS DE PAGINACIÓN
// ============================================================================

t_estructura_paginas* crear_estructura_paginas(int pid, int tamanio) {
    t_estructura_paginas* estructura = malloc(sizeof(t_estructura_paginas));
    if (!estructura) {
        log_error(logger, "PID: %d - Error al asignar memoria para estructura de paginación", pid);
        return NULL;
    }
    
    // Inicializar estructura
    estructura->pid = pid;
    estructura->cantidad_niveles = cfg->CANTIDAD_NIVELES;
    estructura->entradas_por_tabla = cfg->ENTRADAS_POR_TABLA;
    estructura->tam_pagina = cfg->TAM_PAGINA;
    estructura->tamanio_proceso = tamanio;
    estructura->paginas_totales = (tamanio + cfg->TAM_PAGINA - 1) / cfg->TAM_PAGINA;
    
    // Inicializar mutex
    if (pthread_mutex_init(&estructura->mutex_estructura, NULL) != 0) {
        log_error(logger, "PID: %d - Error al inicializar mutex de estructura", pid);
        free(estructura);
        return NULL;
    }
    
    // Crear tabla raíz
    estructura->tabla_raiz = crear_tabla_paginas(cfg->CANTIDAD_NIVELES - 1);
    if (!estructura->tabla_raiz) {
        log_error(logger, "PID: %d - Error al crear tabla raíz", pid);
        pthread_mutex_destroy(&estructura->mutex_estructura);
        free(estructura);
        return NULL;
    }
    
    log_debug(logger, "PID: %d - Estructura de paginación creada - %d páginas, %d niveles", 
              pid, estructura->paginas_totales, estructura->cantidad_niveles);
    return estructura;
}

void destruir_estructura_paginas(t_estructura_paginas* estructura) {
    if (!estructura) return;
    
    if (estructura->tabla_raiz) {
        destruir_tabla_paginas_recursiva(estructura->tabla_raiz);
    }
    
    pthread_mutex_destroy(&estructura->mutex_estructura);
    free(estructura);
}

t_tabla_paginas* crear_tabla_paginas(int nivel) {
    t_tabla_paginas* tabla = malloc(sizeof(t_tabla_paginas));
    if (!tabla) {
        log_error(logger, "Error al asignar memoria para tabla de páginas nivel %d", nivel);
        return NULL;
    }
    
    tabla->nivel = nivel;
    tabla->inicializada = true;
    
    // Asignar array de entradas
    tabla->entradas = malloc(sizeof(t_entrada_tabla) * cfg->ENTRADAS_POR_TABLA);
    if (!tabla->entradas) {
        log_error(logger, "Error al asignar entradas para tabla nivel %d", nivel);
        free(tabla);
        return NULL;
    }
    
    // Inicializar todas las entradas
    for (int i = 0; i < cfg->ENTRADAS_POR_TABLA; i++) {
        tabla->entradas[i].presente = false;
        tabla->entradas[i].modificado = false;
        tabla->entradas[i].numero_frame = 0;
        tabla->entradas[i].tabla_siguiente = NULL;
    }
    
    return tabla;
}

void destruir_tabla_paginas_recursiva(t_tabla_paginas* tabla) {
    if (!tabla) return;
    
    // Si no es nivel hoja, destruir tablas hijas recursivamente
    if (tabla->nivel > 0 && tabla->entradas) {
        for (int i = 0; i < cfg->ENTRADAS_POR_TABLA; i++) {
            if (tabla->entradas[i].tabla_siguiente) {
                destruir_tabla_paginas_recursiva((t_tabla_paginas*)tabla->entradas[i].tabla_siguiente);
            }
        }
    }
    
    if (tabla->entradas) {
        free(tabla->entradas);
    }
    
    free(tabla);
}

t_resultado_memoria configurar_entrada_pagina(t_estructura_paginas* estructura, int numero_pagina, int numero_frame) {
    if (!estructura || !estructura->tabla_raiz) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    pthread_mutex_lock(&estructura->mutex_estructura);
    
    // Calcular índices para cada nivel
    int indices[estructura->cantidad_niveles];
    int pagina_temp = numero_pagina;
    
    for (int nivel = 0; nivel < estructura->cantidad_niveles; nivel++) {
        indices[nivel] = pagina_temp % estructura->entradas_por_tabla;
        pagina_temp /= estructura->entradas_por_tabla;
    }
    
    // Navegar hasta el nivel hoja creando tablas intermedias si es necesario
    t_tabla_paginas* tabla_actual = estructura->tabla_raiz;
    
    for (int nivel = estructura->cantidad_niveles - 1; nivel > 0; nivel--) {
        int indice = indices[nivel];
        
        if (!tabla_actual->entradas[indice].presente) {
            // Crear nueva tabla para el siguiente nivel
            tabla_actual->entradas[indice].tabla_siguiente = crear_tabla_paginas(nivel - 1);
            if (!tabla_actual->entradas[indice].tabla_siguiente) {
                pthread_mutex_unlock(&estructura->mutex_estructura);
                return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
            }
            tabla_actual->entradas[indice].presente = true;
        }
        
        // Verificar si la entrada está presente y tiene tabla siguiente
        if (tabla_actual->entradas[indices[nivel]].tabla_siguiente == NULL ||
            !tabla_actual->entradas[indices[nivel]].presente) {
            log_error(logger, "PID: %d - Página %d no presente en nivel %d", pid, numero_pagina, nivel);
            return -1;
        }
        
        tabla_actual = (t_tabla_paginas*)tabla_actual->entradas[indices[nivel]].tabla_siguiente;
        if (tabla_actual == NULL && nivel < estructura->cantidad_niveles - 2) {
            log_error(logger, "PID: %d - Tabla siguiente es NULL en nivel %d", pid, nivel);
            return -1;
        }
    }
    
    // Acceder a la entrada final (nivel hoja)
    int indice_final = indices[estructura->cantidad_niveles - 1];
    
    log_trace(logger, "PID: %d - Accediendo a entrada final en nivel %d, índice %d", 
              pid, estructura->cantidad_niveles - 1, indice_final);
    
    // Verificar si la página está presente
    if (tabla_actual->entradas[indice_final].tabla_siguiente == NULL ||
        !tabla_actual->entradas[indice_final].presente) {
        log_error(logger, "PID: %d - Página %d no presente en nivel hoja", pid, numero_pagina);
        return -1;
    }
    
    int numero_marco = tabla_actual->entradas[indice_final].numero_frame;
    
    pthread_mutex_unlock(&estructura->mutex_estructura);
    
    log_trace(logger, "PID: %d - Marco obtenido: %d para página %d", pid, numero_marco, numero_pagina);
    return numero_marco;
}

// ============================================================================
// FUNCIONES DE ACCESO A TABLAS DE PÁGINAS
// ============================================================================

int obtener_marco_pagina(int pid, int numero_pagina) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    t_estructura_paginas* estructura = dictionary_get(sistema_memoria->estructuras_paginas, pid_str);
    if (!estructura) {
        log_error(logger, "PID: %d - Estructura de páginas no encontrada", pid);
        return -1;
    }
    
    pthread_mutex_lock(&estructura->mutex_estructura);
    
    // Incrementar métrica de acceso a tabla de páginas
    incrementar_accesos_tabla_paginas(pid);
    
    // Aplicar retardo de memoria por cada nivel accedido
    for (int i = 0; i < estructura->cantidad_niveles; i++) {
        aplicar_retardo_memoria();
    }
    
    // Calcular índices para navegación
    int indices[estructura->cantidad_niveles];
    int pagina_temp = numero_pagina;
    
    for (int nivel = 0; nivel < estructura->cantidad_niveles; nivel++) {
        indices[nivel] = pagina_temp % estructura->entradas_por_tabla;
        pagina_temp /= estructura->entradas_por_tabla;
    }
    
    // Navegar hasta el nivel hoja
    t_tabla_paginas* tabla_actual = estructura->tabla_raiz;
    
    for (int nivel = estructura->cantidad_niveles - 1; nivel > 0; nivel--) {
        int indice = indices[nivel];
        
        if (!tabla_actual->entradas[indice].presente) {
            log_error(logger, "PID: %d - Página %d no presente en nivel %d", pid, numero_pagina, nivel);
            pthread_mutex_unlock(&estructura->mutex_estructura);
            return -1;
        }
        
        tabla_actual = tabla_actual->entradas[indice].tabla_siguiente;
    }
    
    // Obtener frame del nivel hoja
    int indice_final = indices[0];
    if (!tabla_actual->entradas[indice_final].presente) {
        log_error(logger, "PID: %d - Página %d no presente", pid, numero_pagina);
        pthread_mutex_unlock(&estructura->mutex_estructura);
        return -1;
    }
    
    int numero_frame = tabla_actual->entradas[indice_final].numero_frame;
    
    pthread_mutex_unlock(&estructura->mutex_estructura);
    
    log_trace(logger, "## PID: %d - OBTENER MARCO - Página: %d - Marco: %d", pid, numero_pagina, numero_frame);
    return numero_frame;
}

// ============================================================================
// FUNCIONES DE ACCESO A MEMORIA FÍSICA
// ============================================================================

t_resultado_memoria leer_memoria_fisica(int pid, int direccion_fisica, int tamanio, void* buffer) {
    if (!sistema_memoria || !buffer) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    // Verificar que la dirección esté dentro de los límites
    if (direccion_fisica < 0 || direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Dirección física fuera de rango: %d", pid, direccion_fisica);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    // Aplicar retardo de memoria
    aplicar_retardo_memoria();
    
    // Leer desde memoria principal
    memcpy(buffer, (char*)sistema_memoria->memoria_principal + direccion_fisica, tamanio);
    
    // Incrementar métrica de lectura
    incrementar_lecturas_memoria(pid);
    
    log_trace(logger, "## PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", pid, direccion_fisica, tamanio);
    return MEMORIA_OK;
}

t_resultado_memoria escribir_memoria_fisica(int pid, int direccion_fisica, int tamanio, void* datos) {
    if (!sistema_memoria || !datos) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    // Verificar que la dirección esté dentro de los límites
    if (direccion_fisica < 0 || direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Dirección física fuera de rango: %d", pid, direccion_fisica);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    // Aplicar retardo de memoria
    aplicar_retardo_memoria();
    
    // Escribir en memoria principal
    memcpy((char*)sistema_memoria->memoria_principal + direccion_fisica, datos, tamanio);
    
    // Incrementar métrica de escritura
    incrementar_escrituras_memoria(pid);
    
    log_trace(logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", pid, direccion_fisica, tamanio);
    return MEMORIA_OK;
}

// ============================================================================
// FUNCIONES DE UTILIDAD
// ============================================================================

void liberar_marcos_proceso(int pid) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        return;
    }
    
    t_administrador_marcos* admin = sistema_memoria->admin_marcos;
    
    pthread_mutex_lock(&admin->mutex_frames);
    
    // Buscar y liberar todos los marcos del proceso
    for (int i = 0; i < admin->cantidad_total_frames; i++) {
        if (admin->frames[i].ocupado && admin->frames[i].pid_propietario == pid) {
            liberar_marco(i);
        }
    }
    
    pthread_mutex_unlock(&admin->mutex_frames);
    
    log_debug(logger, "PID: %d - Todos los marcos del proceso liberados", pid);
}

void aplicar_retardo_memoria(void) {
    if (cfg->RETARDO_MEMORIA > 0) {
        usleep(cfg->RETARDO_MEMORIA * 1000); // Convertir ms a microsegundos
    }
}

void aplicar_retardo_swap(void) {
    if (cfg->RETARDO_SWAP > 0) {
        usleep(cfg->RETARDO_SWAP * 1000); // Convertir ms a microsegundos
    }
}

bool proceso_existe(int pid) {
    if (!sistema_memoria) {
        return false;
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    return dictionary_has_key(sistema_memoria->procesos, pid_str);
}

// ============================================================================
// FUNCIONES DE ACCESO A MEMORIA SEGÚN CONSIGNA
// ============================================================================

/**
 * 1. ACCESO A TABLA DE PÁGINAS
 * El módulo deberá responder con el número de marco correspondiente. 
 * En este evento se deberá tener en cuenta la cantidad de niveles de tablas 
 * de páginas accedido, debiendo considerar un acceso (con su respectivo conteo 
 * de métricas y retardo de acceso) por cada nivel de tabla de páginas accedido.
 */
int acceso_tabla_paginas(int pid, int numero_pagina) {
    log_trace(logger, "PID: %d - Acceso a tabla de páginas - Página: %d", pid, numero_pagina);
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para acceso a tabla de páginas", pid);
        return -1;
    }
    
    // Obtener estructura de páginas del proceso
    char* pid_key = string_itoa(pid);
    t_estructura_paginas* estructura = dictionary_get(sistema_memoria->estructuras_paginas, pid_key);
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_key);
    free(pid_key);
    
    if (estructura == NULL || metricas == NULL) {
        log_error(logger, "PID: %d - No se encontraron estructuras para acceso a tabla de páginas", pid);
        return -1;
    }
    
    // Navegar por la jerarquía multinivel
    t_tabla_paginas* tabla_actual = estructura->tabla_raiz;
    int indices[estructura->cantidad_niveles];
    
    // Calcular índices para cada nivel
    calcular_indices_multinivel(numero_pagina, estructura->cantidad_niveles, 
                               cfg->ENTRADAS_POR_TABLA, indices);
    
    // Navegar por cada nivel (excepto el último)
    for (int nivel = 0; nivel < estructura->cantidad_niveles - 1; nivel++) {
        // Incrementar métrica de acceso a tabla de páginas
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->accesos_tabla_paginas++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
        
        // Aplicar retardo de acceso a memoria
        usleep(cfg->RETARDO_MEMORIA * 1000);
        
        log_trace(logger, "PID: %d - Accediendo nivel %d, índice %d", pid, nivel, indices[nivel]);
        
        // Verificar que la entrada existe y está presente
        if (indices[nivel] >= cfg->ENTRADAS_POR_TABLA || 
            tabla_actual->entradas[indices[nivel]] == NULL ||
            !tabla_actual->entradas[indices[nivel]]->presente) {
            log_error(logger, "PID: %d - Entrada no presente en nivel %d, índice %d", 
                     pid, nivel, indices[nivel]);
            return -1;
        }
        
        // Avanzar al siguiente nivel
        tabla_actual = tabla_actual->entradas[indices[nivel]]->tabla_siguiente;
        if (tabla_actual == NULL && nivel < estructura->cantidad_niveles - 2) {
            log_error(logger, "PID: %d - Tabla siguiente es NULL en nivel %d", pid, nivel);
            return -1;
        }
    }
    
    // Acceso final al último nivel para obtener el marco
    int indice_final = indices[estructura->cantidad_niveles - 1];
    
    // Incrementar métrica de acceso a tabla de páginas (último nivel)
    pthread_mutex_lock(&metricas->mutex_metricas);
    metricas->accesos_tabla_paginas++;
    pthread_mutex_unlock(&metricas->mutex_metricas);
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    log_trace(logger, "PID: %d - Accediendo nivel final %d, índice %d", 
             pid, estructura->cantidad_niveles - 1, indice_final);
    
    // Verificar entrada final
    if (indice_final >= cfg->ENTRADAS_POR_TABLA || 
        tabla_actual->entradas[indice_final] == NULL ||
        !tabla_actual->entradas[indice_final]->presente) {
        log_error(logger, "PID: %d - Entrada final no presente, índice %d", pid, indice_final);
        return -1;
    }
    
    int numero_marco = tabla_actual->entradas[indice_final]->numero_frame;
    log_trace(logger, "PID: %d - Marco obtenido: %d para página %d", pid, numero_marco, numero_pagina);
    
    return numero_marco;
}

/**
 * 2. ACCESO A ESPACIO DE USUARIO - LECTURA
 * Ante un pedido de lectura, devolver el valor que se encuentra en la posición pedida.
 */
void* acceso_espacio_usuario_lectura(int pid, int direccion_fisica, int tamanio) {
    log_trace(logger, "PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", 
             pid, direccion_fisica, tamanio);
    
    // Validar parámetros
    if (direccion_fisica < 0 || tamanio <= 0) {
        log_error(logger, "PID: %d - Parámetros inválidos para lectura", pid);
        return NULL;
    }
    
    // Validar que la dirección está dentro del espacio de memoria
    if (direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Dirección fuera de rango: %d + %d > %d", 
                 pid, direccion_fisica, tamanio, cfg->TAM_MEMORIA);
        return NULL;
    }
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para lectura", pid);
        return NULL;
    }
    
    // Actualizar métricas
    char* pid_key = string_itoa(pid);
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_key);
    free(pid_key);
    
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->lecturas_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Reservar memoria para el resultado
    void* resultado = malloc(tamanio);
    if (resultado == NULL) {
        log_error(logger, "PID: %d - Error al reservar memoria para lectura", pid);
        return NULL;
    }
    
    // Copiar datos desde el espacio de usuario
    memcpy(resultado, sistema_memoria->memoria_principal + direccion_fisica, tamanio);
    
    log_info(logger, "## PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", 
             pid, direccion_fisica, tamanio);
    
    return resultado;
}

/**
 * 2. ACCESO A ESPACIO DE USUARIO - ESCRITURA
 * Ante un pedido de escritura, escribir lo indicado en la posición pedida. 
 * En caso satisfactorio se responderá un mensaje de 'OK'.
 */
bool acceso_espacio_usuario_escritura(int pid, int direccion_fisica, int tamanio, void* datos) {
    log_trace(logger, "PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", 
             pid, direccion_fisica, tamanio);
    
    // Validar parámetros
    if (direccion_fisica < 0 || tamanio <= 0 || datos == NULL) {
        log_error(logger, "PID: %d - Parámetros inválidos para escritura", pid);
        return false;
    }
    
    // Validar que la dirección está dentro del espacio de memoria
    if (direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Dirección fuera de rango: %d + %d > %d", 
                 pid, direccion_fisica, tamanio, cfg->TAM_MEMORIA);
        return false;
    }
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para escritura", pid);
        return false;
    }
    
    // Actualizar métricas
    char* pid_key = string_itoa(pid);
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_key);
    free(pid_key);
    
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->escrituras_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Escribir datos en el espacio de usuario
    memcpy(sistema_memoria->memoria_principal + direccion_fisica, datos, tamanio);
    
    log_info(logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", 
             pid, direccion_fisica, tamanio);
    
    return true;
}

/**
 * 3. LEER PÁGINA COMPLETA
 * Se deberá devolver el contenido correspondiente de la página a partir del byte 
 * enviado como dirección física dentro de la Memoria de Usuario, que deberá 
 * coincidir con la posición del byte 0 de la página.
 */
void* leer_pagina_completa(int pid, int direccion_fisica) {
    log_trace(logger, "PID: %d - Leer página completa - Dir. Física: %d", pid, direccion_fisica);
    
    // Validar que la dirección coincide con el byte 0 de una página
    if (direccion_fisica % cfg->TAM_PAGINA != 0) {
        log_error(logger, "PID: %d - Dirección física %d no coincide con byte 0 de página (TAM_PAGINA=%d)", 
                 pid, direccion_fisica, cfg->TAM_PAGINA);
        return NULL;
    }
    
    // Validar que la dirección está dentro del espacio de memoria
    if (direccion_fisica + cfg->TAM_PAGINA > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Página fuera de rango: %d + %d > %d", 
                 pid, direccion_fisica, cfg->TAM_PAGINA, cfg->TAM_MEMORIA);
        return NULL;
    }
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para leer página completa", pid);
        return NULL;
    }
    
    // Actualizar métricas
    char* pid_key = string_itoa(pid);
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_key);
    free(pid_key);
    
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->lecturas_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Reservar memoria para la página completa
    void* pagina_completa = malloc(cfg->TAM_PAGINA);
    if (pagina_completa == NULL) {
        log_error(logger, "PID: %d - Error al reservar memoria para página completa", pid);
        return NULL;
    }
    
    // Copiar página completa desde el espacio de usuario
    memcpy(pagina_completa, sistema_memoria->memoria_principal + direccion_fisica, cfg->TAM_PAGINA);
    
    log_trace(logger, "PID: %d - Página completa leída desde dirección física %d", pid, direccion_fisica);
    
    return pagina_completa;
}

/**
 * 4. ACTUALIZAR PÁGINA COMPLETA
 * Se escribirá la página completa a partir del byte 0 que igual será enviado 
 * como dirección física, esta operación se realizará dentro de la Memoria de 
 * Usuario y se responderá como OK.
 */
bool actualizar_pagina_completa(int pid, int direccion_fisica, void* contenido_pagina) {
    log_trace(logger, "PID: %d - Actualizar página completa - Dir. Física: %d", pid, direccion_fisica);
    
    // Validar parámetros
    if (contenido_pagina == NULL) {
        log_error(logger, "PID: %d - Contenido de página es NULL", pid);
        return false;
    }
    
    // Validar que la dirección coincide con el byte 0 de una página
    if (direccion_fisica % cfg->TAM_PAGINA != 0) {
        log_error(logger, "PID: %d - Dirección física %d no coincide con byte 0 de página (TAM_PAGINA=%d)", 
                 pid, direccion_fisica, cfg->TAM_PAGINA);
        return false;
    }
    
    // Validar que la dirección está dentro del espacio de memoria
    if (direccion_fisica + cfg->TAM_PAGINA > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Página fuera de rango: %d + %d > %d", 
                 pid, direccion_fisica, cfg->TAM_PAGINA, cfg->TAM_MEMORIA);
        return false;
    }
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para actualizar página completa", pid);
        return false;
    }
    
    // Actualizar métricas
    char* pid_key = string_itoa(pid);
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_key);
    free(pid_key);
    
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->escrituras_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Escribir página completa en el espacio de usuario
    memcpy(sistema_memoria->memoria_principal + direccion_fisica, contenido_pagina, cfg->TAM_PAGINA);
    
    log_trace(logger, "PID: %d - Página completa actualizada en dirección física %d", pid, direccion_fisica);
    
    return true;
}

// ============================================================================
// FUNCIONES AUXILIARES DE ACCESO A MEMORIA FÍSICA
// ============================================================================

void* leer_pagina_memoria(int numero_frame) {
    if (!sistema_memoria || !sistema_memoria->memoria_principal) {
        log_error(logger, "Sistema de memoria no inicializado");
        return NULL;
    }
    
    if (numero_frame < 0 || numero_frame >= sistema_memoria->admin_marcos->cantidad_total_frames) {
        log_error(logger, "Número de frame inválido: %d", numero_frame);
        return NULL;
    }
    
    // Asignar memoria para el contenido de la página
    void* contenido = malloc(cfg->TAM_PAGINA);
    if (!contenido) {
        log_error(logger, "Error al asignar memoria para leer página del frame %d", numero_frame);
        return NULL;
    }
    
    // Calcular dirección física del frame
    uint32_t direccion_fisica = numero_frame * cfg->TAM_PAGINA;
    
    // Copiar contenido desde memoria principal
    memcpy(contenido, sistema_memoria->memoria_principal + direccion_fisica, cfg->TAM_PAGINA);
    
    // Aplicar retardo de memoria
    aplicar_retardo_memoria();
    
    log_trace(logger, "Página leída desde frame %d (dirección física %d)", numero_frame, direccion_fisica);
    
    return contenido;
}

int escribir_pagina_memoria(int numero_frame, void* contenido) {
    if (!sistema_memoria || !sistema_memoria->memoria_principal) {
        log_error(logger, "Sistema de memoria no inicializado");
        return 0;
    }
    
    if (numero_frame < 0 || numero_frame >= sistema_memoria->admin_marcos->cantidad_total_frames) {
        log_error(logger, "Número de frame inválido: %d", numero_frame);
        return 0;
    }
    
    if (!contenido) {
        log_error(logger, "Contenido nulo para escribir en frame %d", numero_frame);
        return 0;
    }
    
    // Calcular dirección física del frame
    uint32_t direccion_fisica = numero_frame * cfg->TAM_PAGINA;
    
    // Copiar contenido a memoria principal
    memcpy(sistema_memoria->memoria_principal + direccion_fisica, contenido, cfg->TAM_PAGINA);
    
    // Aplicar retardo de memoria
    aplicar_retardo_memoria();
    
    log_trace(logger, "Página escrita en frame %d (dirección física %d)", numero_frame, direccion_fisica);
    
    return 1;
} 