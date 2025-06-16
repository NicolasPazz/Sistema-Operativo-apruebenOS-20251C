#include "../headers/manejo_swap.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/metricas.h"
#include "../headers/manejo_memoria.h"
#include <commons/log.h>
#include <commons/string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

// Variables externas
extern t_sistema_memoria* sistema_memoria;
extern t_log* logger;
extern t_config_memoria* cfg;

// ============================================================================
// FUNCIONES DE SUSPENSIÓN Y REANUDACIÓN DE PROCESOS
// ============================================================================

int suspender_proceso_completo(int pid) {
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe, no se puede suspender", pid);
        return 0;
    }
    
    log_trace(logger, "PID: %d - Iniciando suspensión completa del proceso", pid);
    
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, string_itoa(pid));
    if (proceso == NULL || proceso->suspendido) {
        log_warning(logger, "PID: %d - Proceso ya suspendido o no encontrado", pid);
        return 0;
    }
    
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Obtener estructura de páginas del proceso
    t_estructura_paginas* estructura = proceso->estructura_paginas;
    if (estructura == NULL) {
        log_error(logger, "PID: %d - No se encontró estructura de páginas", pid);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Verificar espacio en SWAP
    if (!asignar_espacio_swap_proceso(pid)) {
        log_error(logger, "PID: %d - No hay suficiente espacio en SWAP", pid);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Escribir TODAS las páginas del proceso a SWAP secuencialmente
    int paginas_escritas = 0;
    for (int i = 0; i < estructura->paginas_totales; i++) {
        // Buscar la entrada de tabla correspondiente a esta página
        t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, i);
        
        if (entrada != NULL && entrada->presente) {
            // Obtener contenido de la página desde memoria principal
            void* contenido_pagina = malloc(cfg->TAM_PAGINA);
            if (contenido_pagina == NULL) {
                log_error(logger, "Error al asignar memoria para página %d del proceso %d", i, pid);
                continue;
            }
            
            if (leer_pagina_memoria(entrada->numero_frame, contenido_pagina) != MEMORIA_OK) {
                log_error(logger, "Error al leer página %d del proceso %d", i, pid);
                free(contenido_pagina);
                continue;
            }
            
            // Escribir página a SWAP secuencialmente
            if (write(sistema_memoria->admin_swap->fd_swap, contenido_pagina, cfg->TAM_PAGINA) != cfg->TAM_PAGINA) {
                log_error(logger, "Error al escribir página %d del proceso %d a SWAP", i, pid);
                free(contenido_pagina);
                pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
                return 0;
            }
            
            // Liberar frame en memoria principal
            liberar_frame(entrada->numero_frame);
            
            // Marcar página como NO presente (ya no está en memoria)
            entrada->presente = false;
            entrada->numero_frame = 0;
            
            paginas_escritas++;
            free(contenido_pagina);
        }
    }
    
    // Marcar proceso como suspendido
    proceso->suspendido = true;
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Incrementar métrica de bajadas a SWAP
    incrementar_bajadas_swap(pid);
    
    // Aplicar retardo de SWAP
    aplicar_retardo_swap();
    
    log_trace(logger, "PID: %d - Proceso suspendido completamente. Páginas escritas a SWAP: %d", 
             pid, paginas_escritas);
    
    return 1;
}

int reanudar_proceso_suspendido(int pid) {
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe, no se puede reanudar", pid);
        return 0;
    }
    
    log_trace(logger, "PID: %d - Iniciando reanudación del proceso suspendido", pid);
    
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, string_itoa(pid));
    if (proceso == NULL || !proceso->suspendido) {
        log_warning(logger, "PID: %d - Proceso no está suspendido", pid);
        return 0;
    }
    
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Obtener estructura de páginas del proceso
    t_estructura_paginas* estructura = proceso->estructura_paginas;
    if (estructura == NULL) {
        log_error(logger, "PID: %d - No se encontró estructura de páginas", pid);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Verificar si hay espacio suficiente en memoria principal
    if (sistema_memoria->admin_marcos->frames_libres < estructura->paginas_totales) {
        log_error(logger, "PID: %d - No hay suficiente espacio en memoria para reanudar proceso", pid);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Posicionarse al inicio del área del proceso en SWAP
    off_t offset_inicio = 0; // Simplificado: cada proceso se escribe secuencialmente
    if (lseek(sistema_memoria->admin_swap->fd_swap, offset_inicio, SEEK_SET) == -1) {
        log_error(logger, "PID: %d - Error al posicionarse en SWAP", pid);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Leer TODAS las páginas desde SWAP y cargarlas en memoria principal
    int paginas_cargadas = 0;
    
    for (int i = 0; i < estructura->paginas_totales; i++) {
        t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, i);
        
        if (entrada != NULL && !entrada->presente) {
            // Asignar nuevo frame en memoria principal
            int nuevo_frame = asignar_frame_libre(pid, i);
            if (nuevo_frame == -1) {
                log_error(logger, "PID: %d - No se pudo asignar frame para página %d", pid, i);
                continue;
            }
            
            // Leer contenido desde SWAP secuencialmente
            void* contenido = malloc(cfg->TAM_PAGINA);
            if (contenido == NULL) {
                log_error(logger, "PID: %d - Error al asignar memoria para página %d", pid, i);
                continue;
            }
            
            if (read(sistema_memoria->admin_swap->fd_swap, contenido, cfg->TAM_PAGINA) != cfg->TAM_PAGINA) {
                log_error(logger, "PID: %d - Error al leer página %d desde SWAP", pid, i);
                free(contenido);
                continue;
            }
            
            // Escribir contenido en memoria principal
            if (escribir_pagina_memoria(nuevo_frame, contenido) != MEMORIA_OK) {
                log_error(logger, "PID: %d - Error al escribir página %d en memoria", pid, i);
                free(contenido);
                continue;
            }
            
            // Actualizar entrada de tabla
            entrada->presente = true;
            entrada->numero_frame = nuevo_frame;
            
            paginas_cargadas++;
            free(contenido);
        }
    }
    
    // Liberar espacio en SWAP (ya no se necesita)
    liberar_espacio_swap_proceso(pid);
    
    // Marcar proceso como activo
    proceso->suspendido = false;
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Incrementar métrica de subidas a memoria principal
    incrementar_subidas_memoria_principal(pid);
    
    // Aplicar retardo de SWAP
    aplicar_retardo_swap();
    
    log_trace(logger, "PID: %d - Proceso reanudado exitosamente. Páginas cargadas desde SWAP: %d", 
             pid, paginas_cargadas);
    
    return 1;
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE PÁGINAS EN SWAP
// ============================================================================

int escribir_pagina_proceso_swap(int pid, int numero_pagina, void* contenido) {
    if (contenido == NULL) {
        log_error(logger, "PID: %d - Contenido nulo para escribir página %d", pid, numero_pagina);
        return 0;
    }
    
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Buscar posición libre en SWAP
    int posicion_libre = -1;
    for (int i = 0; i < sistema_memoria->admin_swap->cantidad_paginas_swap; i++) {
        if (!sistema_memoria->admin_swap->entradas[i].ocupado) {
            posicion_libre = i;
            break;
        }
    }
    
    if (posicion_libre == -1) {
        log_error(logger, "PID: %d - No hay espacio libre en SWAP para página %d", pid, numero_pagina);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Calcular offset en el archivo SWAP
    off_t offset = posicion_libre * cfg->TAM_PAGINA;
    
    // Posicionarse en el offset correcto
    if (lseek(sistema_memoria->admin_swap->fd_swap, offset, SEEK_SET) == -1) {
        log_error(logger, "PID: %d - Error al posicionarse en SWAP para página %d", pid, numero_pagina);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Escribir contenido en el archivo SWAP
    if (write(sistema_memoria->admin_swap->fd_swap, contenido, cfg->TAM_PAGINA) != cfg->TAM_PAGINA) {
        log_error(logger, "PID: %d - Error al escribir página %d en SWAP", pid, numero_pagina);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Actualizar entrada de SWAP
    sistema_memoria->admin_swap->entradas[posicion_libre].ocupado = true;
    sistema_memoria->admin_swap->entradas[posicion_libre].pid_propietario = pid;
    sistema_memoria->admin_swap->entradas[posicion_libre].numero_pagina = numero_pagina;
    
    // Actualizar contador
    sistema_memoria->admin_swap->paginas_libres_swap--;
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Aplicar retardo de SWAP
    aplicar_retardo_swap();
    
    log_trace(logger, "PID: %d - Página %d escrita en SWAP posición %d", 
              pid, numero_pagina, posicion_libre);
    
    return 1;
}

void* leer_pagina_proceso_swap(int pid, int numero_pagina) {
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Buscar la página en SWAP
    int posicion_swap = -1;
    for (int i = 0; i < sistema_memoria->admin_swap->cantidad_paginas_swap; i++) {
        if (sistema_memoria->admin_swap->entradas[i].ocupado && 
            sistema_memoria->admin_swap->entradas[i].pid_propietario == pid &&
            sistema_memoria->admin_swap->entradas[i].numero_pagina == numero_pagina) {
            posicion_swap = i;
            break;
        }
    }
    
    if (posicion_swap == -1) {
        log_error(logger, "PID: %d - Página %d no encontrada en SWAP", pid, numero_pagina);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return NULL;
    }
    
    // Asignar memoria para el contenido
    void* contenido = malloc(cfg->TAM_PAGINA);
    if (contenido == NULL) {
        log_error(logger, "PID: %d - Error al asignar memoria para leer página %d", pid, numero_pagina);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return NULL;
    }
    
    // Calcular offset y leer desde el archivo SWAP
    off_t offset = posicion_swap * cfg->TAM_PAGINA;
    
    if (lseek(sistema_memoria->admin_swap->fd_swap, offset, SEEK_SET) == -1) {
        log_error(logger, "PID: %d - Error al posicionarse en SWAP para leer página %d", pid, numero_pagina);
        free(contenido);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return NULL;
    }
    
    if (read(sistema_memoria->admin_swap->fd_swap, contenido, cfg->TAM_PAGINA) != cfg->TAM_PAGINA) {
        log_error(logger, "PID: %d - Error al leer página %d desde SWAP", pid, numero_pagina);
        free(contenido);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return NULL;
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Aplicar retardo de SWAP
    aplicar_retardo_swap();
    
    log_trace(logger, "PID: %d - Página %d leída desde SWAP posición %d", 
              pid, numero_pagina, posicion_swap);
    
    return contenido;
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE ESPACIO EN SWAP
// ============================================================================

int asignar_espacio_swap_proceso(int pid) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    
    if (proceso == NULL) {
        log_error(logger, "PID: %d - Proceso no encontrado para asignar SWAP", pid);
        return 0;
    }
    
    int paginas_necesarias = proceso->estructura_paginas->paginas_totales;
    
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    if (sistema_memoria->admin_swap->paginas_libres_swap < paginas_necesarias) {
        log_error(logger, "PID: %d - No hay suficiente espacio en SWAP (%d páginas necesarias, %d disponibles)", 
                  pid, paginas_necesarias, sistema_memoria->admin_swap->paginas_libres_swap);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    log_trace(logger, "PID: %d - Espacio en SWAP verificado: %d páginas disponibles", 
              pid, paginas_necesarias);
    
    return 1;
}

void liberar_espacio_swap_proceso(int pid) {
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    int paginas_liberadas = 0;
    
    // Buscar y liberar todas las páginas del proceso en SWAP
    for (int i = 0; i < sistema_memoria->admin_swap->cantidad_paginas_swap; i++) {
        if (sistema_memoria->admin_swap->entradas[i].ocupado &&
            sistema_memoria->admin_swap->entradas[i].pid_propietario == pid) {
            
            // Limpiar entrada
            sistema_memoria->admin_swap->entradas[i].ocupado = false;
            sistema_memoria->admin_swap->entradas[i].pid_propietario = -1;
            sistema_memoria->admin_swap->entradas[i].numero_pagina = -1;
            
            // Actualizar contador
            sistema_memoria->admin_swap->paginas_libres_swap++;
            paginas_liberadas++;
        }
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    log_trace(logger, "PID: %d - Liberadas %d páginas de SWAP", pid, paginas_liberadas);
}

bool proceso_tiene_paginas_en_swap(int pid) {
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    for (int i = 0; i < sistema_memoria->admin_swap->cantidad_paginas_swap; i++) {
        if (sistema_memoria->admin_swap->entradas[i].ocupado &&
            sistema_memoria->admin_swap->entradas[i].pid_propietario == pid) {
            pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
            return true;
        }
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    return false;
} 