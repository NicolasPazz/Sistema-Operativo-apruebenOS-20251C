#include "../headers/bloqueo_paginas.h"
#include "../headers/monitor_memoria.h"
#include "../headers/init_memoria.h"
#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/dictionary.h>
#include <string.h>
#include <errno.h>

extern t_log* logger;
extern t_sistema_memoria* sistema_memoria;
extern t_config_memoria* cfg;

// ============================================================================
// FUNCIONES HELPER INTERNAS
// ============================================================================

/**
 * @brief Obtiene el puntero al marco físico por su número
 */
static t_frame* obtener_marco(int numero_frame) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        log_error(logger, "Sistema de memoria no inicializado");
        return NULL;
    }
    
    if (numero_frame < 0 || numero_frame >= sistema_memoria->admin_marcos->cantidad_total_frames) {
        log_error(logger, "Número de marco inválido: %d", numero_frame);
        return NULL;
    }
    
    return &sistema_memoria->admin_marcos->frames[numero_frame];
}

/**
 * @brief Busca todos los marcos físicos de un proceso (función interna para bloqueo)
 */
static int buscar_marcos_internos_proceso(int pid, int* marcos_out, int max_marcos) {
    if (!marcos_out || max_marcos <= 0) {
        return -1;
    }
    
    int cantidad_encontrados = 0;
    
    for (int i = 0; i < sistema_memoria->admin_marcos->cantidad_total_frames && cantidad_encontrados < max_marcos; i++) {
        t_frame* frame = &sistema_memoria->admin_marcos->frames[i];
        
        if (frame->ocupado && frame->pid_propietario == pid) {
            marcos_out[cantidad_encontrados] = i;
            cantidad_encontrados++;
        }
    }
    
    return cantidad_encontrados;
}

// ============================================================================
// FUNCIONES PRINCIPALES DE BLOQUEO DE MARCOS FÍSICOS
// ============================================================================

bool bloquear_marco(int numero_frame, const char* operacion) {
    log_trace(logger, "Intentando bloquear marco %d para operación: %s", 
              numero_frame, operacion ? operacion : "DESCONOCIDA");
    
    t_frame* frame = obtener_marco(numero_frame);
    if (!frame) {
        log_error(logger, "No se pudo obtener marco %d", numero_frame);
        return false;
    }
    
    // INICIALIZACIÓN LAZY DEL MUTEX
    if (!frame->mutex_inicializado) {
        int resultado = pthread_mutex_init(&frame->mutex_frame, NULL);
        if (resultado != 0) {
            log_error(logger, "Error al inicializar mutex de marco %d: %s", 
                      numero_frame, strerror(resultado));
            return false;
        }
        frame->mutex_inicializado = true;
        log_trace(logger, "Mutex de marco %d inicializado (lazy)", numero_frame);
    }
    
    // INTENTAR BLOQUEAR
    int resultado_lock = pthread_mutex_lock(&frame->mutex_frame);
    if (resultado_lock != 0) {
        log_error(logger, "Error al obtener lock de marco %d: %s", 
                  numero_frame, strerror(resultado_lock));
        return false;
    }
    
    // VERIFICAR SI YA ESTÁ BLOQUEADO
    if (frame->bloqueado) {
        pthread_t thread_actual = pthread_self();
        if (frame->thread_bloqueador == thread_actual) {
            log_warning(logger, "Marco %d ya bloqueado por el mismo thread (reentrant)", numero_frame);
            pthread_mutex_unlock(&frame->mutex_frame);
            return true; // Permitir re-bloqueo del mismo thread
        } else {
            log_warning(logger, "Marco %d ya está bloqueado por otro thread", numero_frame);
            pthread_mutex_unlock(&frame->mutex_frame);
            return false;
        }
    }
    
    // ESTABLECER BLOQUEO
    frame->bloqueado = true;
    frame->thread_bloqueador = pthread_self();
    if (operacion) {
        strncpy(frame->operacion_actual, operacion, 63);
        frame->operacion_actual[63] = '\0';
    } else {
        strcpy(frame->operacion_actual, "OPERACION_DESCONOCIDA");
    }
    
    pthread_mutex_unlock(&frame->mutex_frame);
    
    log_trace(logger, "Marco %d bloqueado exitosamente para: %s", numero_frame, frame->operacion_actual);
    return true;
}

bool desbloquear_marco(int numero_frame, const char* operacion) {
    log_trace(logger, "Intentando desbloquear marco %d (operación: %s)", 
              numero_frame, operacion ? operacion : "DESCONOCIDA");
    
    t_frame* frame = obtener_marco(numero_frame);
    if (!frame) {
        log_error(logger, "No se pudo obtener marco %d", numero_frame);
        return false;
    }
    
    // SI EL MUTEX NO ESTÁ INICIALIZADO, NO PUEDE ESTAR BLOQUEADO
    if (!frame->mutex_inicializado) {
        log_trace(logger, "Marco %d no tiene mutex inicializado - no puede estar bloqueado", numero_frame);
        return true;
    }
    
    // OBTENER LOCK
    int resultado_lock = pthread_mutex_lock(&frame->mutex_frame);
    if (resultado_lock != 0) {
        log_error(logger, "Error al obtener lock de marco %d: %s", 
                  numero_frame, strerror(resultado_lock));
        return false;
    }
    
    // VERIFICAR QUE ESTÉ BLOQUEADO
    if (!frame->bloqueado) {
        log_trace(logger, "Marco %d no estaba bloqueado", numero_frame);
        pthread_mutex_unlock(&frame->mutex_frame);
        return true;
    }
    
    // VERIFICAR OWNERSHIP DEL BLOQUEO
    pthread_t thread_actual = pthread_self();
    if (frame->thread_bloqueador != thread_actual) {
        log_warning(logger, "Marco %d está bloqueado por otro thread - no se puede desbloquear", numero_frame);
        pthread_mutex_unlock(&frame->mutex_frame);
        return false;
    }
    
    // LIBERAR BLOQUEO
    frame->bloqueado = false;
    frame->thread_bloqueador = 0;
    memset(frame->operacion_actual, 0, 64);
    
    pthread_mutex_unlock(&frame->mutex_frame);
    
    log_trace(logger, "Marco %d desbloqueado exitosamente", numero_frame);
    return true;
}

bool marco_esta_bloqueado(int numero_frame) {
    t_frame* frame = obtener_marco(numero_frame);
    if (!frame) {
        return false;
    }
    
    // Si el mutex no está inicializado, no puede estar bloqueado
    if (!frame->mutex_inicializado) {
        return false;
    }
    
    // Verificación thread-safe del estado de bloqueo
    pthread_mutex_lock(&frame->mutex_frame);
    bool bloqueado = frame->bloqueado;
    pthread_mutex_unlock(&frame->mutex_frame);
    
    return bloqueado;
}

// ============================================================================
// FUNCIONES DE BLOQUEO MASIVO POR PROCESO
// ============================================================================

int bloquear_marcos_proceso(int pid, const char* operacion) {
    log_trace(logger, "PID: %d - Iniciando bloqueo masivo de marcos para: %s", 
              pid, operacion ? operacion : "OPERACION_DESCONOCIDA");
    
    // Obtener lista de marcos del proceso
    int marcos_proceso[256]; // Buffer suficiente para la mayoría de casos
    int cantidad_marcos = buscar_marcos_internos_proceso(pid, marcos_proceso, 256);
    
    if (cantidad_marcos <= 0) {
        log_trace(logger, "PID: %d - No se encontraron marcos para bloquear", pid);
        return 0;
    }
    
    int marcos_bloqueados = 0;
    
    // Intentar bloquear cada marco
    for (int i = 0; i < cantidad_marcos; i++) {
        if (bloquear_marco(marcos_proceso[i], operacion)) {
            marcos_bloqueados++;
        } else {
            log_warning(logger, "PID: %d - No se pudo bloquear marco %d", pid, marcos_proceso[i]);
        }
    }
    
    log_info(logger, "PID: %d - Bloqueo masivo completado: %d/%d marcos bloqueados", 
             pid, marcos_bloqueados, cantidad_marcos);
    
    return marcos_bloqueados;
}

int desbloquear_marcos_proceso(int pid, const char* operacion) {
    log_trace(logger, "PID: %d - Iniciando desbloqueo masivo de marcos para: %s", 
              pid, operacion ? operacion : "OPERACION_DESCONOCIDA");
    
    // Obtener lista de marcos del proceso
    int marcos_proceso[256];
    int cantidad_marcos = buscar_marcos_internos_proceso(pid, marcos_proceso, 256);
    
    if (cantidad_marcos <= 0) {
        log_trace(logger, "PID: %d - No se encontraron marcos para desbloquear", pid);
        return 0;
    }
    
    int marcos_desbloqueados = 0;
    
    // Intentar desbloquear cada marco
    for (int i = 0; i < cantidad_marcos; i++) {
        if (desbloquear_marco(marcos_proceso[i], operacion)) {
            marcos_desbloqueados++;
        } else {
            log_warning(logger, "PID: %d - No se pudo desbloquear marco %d", pid, marcos_proceso[i]);
        }
    }
    
    log_info(logger, "PID: %d - Desbloqueo masivo completado: %d/%d marcos desbloqueados", 
             pid, marcos_desbloqueados, cantidad_marcos);
    
    return marcos_desbloqueados;
}

// ============================================================================
// FUNCIONES DE UTILIDAD Y HELPERS
// ============================================================================

/**
 * @brief Obtiene el número de marco físico de una página específica
 */
static int obtener_numero_marco_de_pagina(int pid, int numero_pagina) {
    // Usar la función existente de monitor_memoria para buscar la entrada
    char* pid_str = string_itoa(pid);
    t_estructura_paginas* estructura = dictionary_get(sistema_memoria->estructuras_paginas, pid_str);
    free(pid_str);
    
    if (!estructura) {
        log_error(logger, "PID: %d - No se encontró estructura de páginas", pid);
        return -1;
    }
    
    t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, numero_pagina);
    if (!entrada) {
        log_error(logger, "PID: %d - No se encontró entrada para página %d", pid, numero_pagina);
        return -1;
    }
    
    if (!entrada->presente) {
        log_trace(logger, "PID: %d - Página %d no está presente en memoria (flujo normal de swap/suspensión)", pid, numero_pagina);
        return -1;
    }
    
    return entrada->numero_frame;
}

/**
 * @brief Obtiene el número de página de un proceso que está mapeada a un marco específico
 */
int obtener_numero_pagina_de_marco(int pid, int numero_marco) {
    char* pid_str = string_itoa(pid);
    t_estructura_paginas* estructura = dictionary_get(sistema_memoria->estructuras_paginas, pid_str);
    free(pid_str);
    
    if (!estructura) {
        log_error(logger, "PID: %d - No se encontró estructura de páginas", pid);
        return -1;
    }
    
    // Buscar la página que está mapeada al marco
    for (int pag = 0; pag < estructura->paginas_totales; pag++) {
        t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, pag);
        if (entrada && entrada->presente && entrada->numero_frame == numero_marco) {
            return pag;
        }
    }
    
    log_error(logger, "PID: %d - No se encontró página mapeada al marco %d", pid, numero_marco);
    return -1;
}

bool bloquear_marco_por_pagina(int pid, int numero_pagina, const char* operacion) {
    log_trace(logger, "PID: %d - Intentando bloquear marco de página %d para: %s", 
              pid, numero_pagina, operacion ? operacion : "OPERACION_DESCONOCIDA");
    
    int numero_frame = obtener_numero_marco_de_pagina(pid, numero_pagina);
    if (numero_frame < 0) {
        log_trace(logger, "PID: %d - No se pudo obtener marco de página %d (flujo normal de swap/suspensión)", pid, numero_pagina);
        return false;
    }
    
    return bloquear_marco(numero_frame, operacion);
}

bool desbloquear_marco_por_pagina(int pid, int numero_pagina, const char* operacion) {
    log_trace(logger, "PID: %d - Intentando desbloquear marco de página %d para: %s", 
              pid, numero_pagina, operacion ? operacion : "OPERACION_DESCONOCIDA");
    
    int numero_frame = obtener_numero_marco_de_pagina(pid, numero_pagina);
    if (numero_frame < 0) {
        log_trace(logger, "PID: %d - No se pudo obtener marco de página %d (flujo normal de swap/suspensión)", pid, numero_pagina);
        return false;
    }
    
    return desbloquear_marco(numero_frame, operacion);
}

bool marco_pagina_esta_bloqueado(int pid, int numero_pagina) {
    int numero_frame = obtener_numero_marco_de_pagina(pid, numero_pagina);
    if (numero_frame < 0) {
        return false;
    }
    
    return marco_esta_bloqueado(numero_frame);
}

pthread_t obtener_thread_bloqueador_marco(int numero_frame) {
    t_frame* frame = obtener_marco(numero_frame);
    if (!frame || !frame->mutex_inicializado) {
        return 0;
    }
    
    pthread_mutex_lock(&frame->mutex_frame);
    pthread_t bloqueador = frame->bloqueado ? frame->thread_bloqueador : 0;
    pthread_mutex_unlock(&frame->mutex_frame);
    
    return bloqueador;
}

bool obtener_operacion_marco(int numero_frame, char* operacion_out) {
    if (!operacion_out) {
        return false;
    }
    
    t_frame* frame = obtener_marco(numero_frame);
    if (!frame || !frame->mutex_inicializado) {
        return false;
    }
    
    pthread_mutex_lock(&frame->mutex_frame);
    if (frame->bloqueado) {
        strncpy(operacion_out, frame->operacion_actual, 63);
        operacion_out[63] = '\0';
    } else {
        operacion_out[0] = '\0';
    }
    pthread_mutex_unlock(&frame->mutex_frame);
    
    return frame->bloqueado;
}

void obtener_estadisticas_bloqueos_marcos(int* marcos_bloqueados_out, int* procesos_con_bloqueos_out) {
    if (!marcos_bloqueados_out || !procesos_con_bloqueos_out) {
        return;
    }
    
    int marcos_bloqueados = 0;
    t_dictionary* procesos_con_bloqueos = dictionary_create();
    
    for (int i = 0; i < sistema_memoria->admin_marcos->cantidad_total_frames; i++) {
        t_frame* frame = &sistema_memoria->admin_marcos->frames[i];
        
        if (frame->mutex_inicializado && marco_esta_bloqueado(i)) {
            marcos_bloqueados++;
            
            if (frame->ocupado && frame->pid_propietario != -1) {
                char pid_str[16];
                snprintf(pid_str, 16, "%d", frame->pid_propietario);
                dictionary_put(procesos_con_bloqueos, pid_str, (void*)1);
            }
        }
    }
    
    *marcos_bloqueados_out = marcos_bloqueados;
    *procesos_con_bloqueos_out = dictionary_size(procesos_con_bloqueos);
    
    dictionary_destroy(procesos_con_bloqueos);
}

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN Y LIMPIEZA
// ============================================================================

bool inicializar_bloqueo_marco(int numero_frame) {
    t_frame* frame = obtener_marco(numero_frame);
    if (!frame) {
        return false;
    }
    
    // Solo inicializar estado básico - el mutex se inicializa lazy
    frame->bloqueado = false;
    frame->thread_bloqueador = 0;
    frame->mutex_inicializado = false;
    memset(frame->operacion_actual, 0, 64);
    
    log_trace(logger, "Estado de bloqueo de marco %d inicializado (mutex lazy)", numero_frame);
    return true;
}

void destruir_bloqueo_marco(int numero_frame) {
    t_frame* frame = obtener_marco(numero_frame);
    if (!frame) {
        return;
    }
    
    // Solo procesar si el mutex está inicializado
    if (!frame->mutex_inicializado) {
        log_trace(logger, "Marco %d con mutex no inicializado - no requiere destrucción", numero_frame);
        return;
    }
    
    // Si el marco está bloqueado, forzar el desbloqueo con warning
    if (frame->bloqueado) {
        log_warning(logger, "Destruyendo marco %d con bloqueo activo - forzando desbloqueo", numero_frame);
        frame->bloqueado = false;
        frame->thread_bloqueador = 0;
        memset(frame->operacion_actual, 0, 64);
    }
    
    // Destruir el mutex
    int resultado = pthread_mutex_destroy(&frame->mutex_frame);
    if (resultado != 0) {
        log_warning(logger, "Error al destruir mutex de marco %d: %s", numero_frame, strerror(resultado));
    } else {
        log_trace(logger, "Mutex de marco %d destruido exitosamente", numero_frame);
    }
    
    frame->mutex_inicializado = false;
} 