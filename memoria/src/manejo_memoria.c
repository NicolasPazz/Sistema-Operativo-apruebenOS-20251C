#include "../headers/manejo_memoria.h"
#include "../headers/interfaz_memoria.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/metricas.h"
#include "../headers/manejo_swap.h"
#include "../headers/utils.h"
#include "../headers/monitor_memoria.h"
#include "../headers/bloqueo_paginas.h"
#include <commons/log.h>
#include <commons/string.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Variables externas
extern t_sistema_memoria* sistema_memoria;
extern t_log* logger;
extern t_config_memoria* cfg;

// Declaraciones de funciones auxiliares internas
static void liberar_marcos_proceso(int pid);

/**
 * @brief Busca la entrada de tabla correspondiente a una página específica
 * Función auxiliar para navegación en jerarquía multinivel
 */
static t_entrada_tabla* _buscar_entrada_pagina(int pid, int numero_pagina) {
    if (!proceso_existe(pid)) {
        return NULL;
    }
    
    char* pid_str = string_itoa(pid);
    t_estructura_paginas* estructura = dictionary_get(sistema_memoria->estructuras_paginas, pid_str);
    free(pid_str);
    
    if (!estructura || !estructura->tabla_raiz) {
        return NULL;
    }
    
    return buscar_entrada_tabla(estructura, numero_pagina);
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE PROCESOS EN MEMORIA
// ============================================================================

/**
 * Crea la estructura básica de un proceso en memoria sin asignar marcos físicos
 * Esta función crea todas las estructuras administrativas necesarias
 */
t_proceso_memoria* crear_proceso_memoria(int pid, int tamanio) {
    if (pid < 0) {
        log_error(logger, "PID: %d - Error al crear proceso: Parámetros inválidos (tamaño=%d)", pid, tamanio);
        return NULL;
    }

    t_proceso_memoria* proceso = malloc(sizeof(t_proceso_memoria));
    if (!proceso) {
        log_error(logger, "PID: %d - Error al asignar memoria para estructura de proceso", pid);
        return NULL;
    }

    // Inicializar campos básicos
    proceso->pid = pid;
    proceso->tamanio = tamanio;
    proceso->nombre_archivo = NULL;  // Se asignará posteriormente si es necesario
    proceso->activo = true;
    proceso->suspendido = false;
    proceso->timestamp_creacion = time(NULL);
    proceso->timestamp_ultimo_uso = time(NULL);

    // Crear estructura de páginas
    proceso->estructura_paginas = crear_estructura_paginas(pid, tamanio);
    if (!proceso->estructura_paginas) {
        log_error(logger, "PID: %d - Error al crear estructura de páginas", pid);
        free(proceso);
        return NULL;
    }

    // Crear métricas
    proceso->metricas = crear_metricas_proceso(pid);
    if (!proceso->metricas) {
        log_error(logger, "PID: %d - Error al crear métricas del proceso", pid);
        destruir_estructura_paginas(proceso->estructura_paginas);
        free(proceso);
        return NULL;
    }

    // Inicializar lista de instrucciones
    proceso->instrucciones = list_create();
    if (!proceso->instrucciones) {
        log_error(logger, "PID: %d - Error al crear lista de instrucciones", pid);
        destruir_metricas_proceso(proceso->metricas);
        destruir_estructura_paginas(proceso->estructura_paginas);
        free(proceso);
        return NULL;
    }

    // Forzar que todas las páginas estén presentes al crear el proceso
    int paginas_totales = proceso->estructura_paginas->paginas_totales;
    int marcos_disponibles = sistema_memoria->admin_marcos->frames_libres;
    if (marcos_disponibles < paginas_totales) {
        log_error(logger, "PID: %d - No hay marcos suficientes para inicializar todas las páginas (%d requeridas, %d libres)", pid, paginas_totales, marcos_disponibles);
        list_destroy_and_destroy_elements(proceso->instrucciones, free);
        destruir_metricas_proceso(proceso->metricas);
        destruir_estructura_paginas(proceso->estructura_paginas);
        free(proceso);
        return NULL;
    }
    for (int i = 0; i < paginas_totales; i++) {
        int frame = asignar_marco_libre(pid, i);
        if (frame == -1) {
            log_error(logger, "PID: %d - Error al asignar marco para página %d al crear proceso", pid, i);
            // Liberar recursos ya asignados
            for (int j = 0; j < i; j++) {
                t_entrada_tabla* entrada = buscar_entrada_tabla(proceso->estructura_paginas, j);
                if (entrada && entrada->presente) {
                    liberar_marco(entrada->numero_frame);
                    entrada->presente = false;
                    entrada->numero_frame = 0;
                }
            }
            list_destroy_and_destroy_elements(proceso->instrucciones, free);
            destruir_metricas_proceso(proceso->metricas);
            destruir_estructura_paginas(proceso->estructura_paginas);
            free(proceso);
            return NULL;
        }
        t_entrada_tabla* entrada = buscar_entrada_tabla(proceso->estructura_paginas, i);
        if (entrada) {
            entrada->presente = true;
            entrada->numero_frame = frame;
            entrada->timestamp_acceso = time(NULL);
        }
    }
    return proceso;
}

t_resultado_memoria crear_proceso_en_memoria(int pid, int tamanio, char* nombre_archivo) {
    // ========== VALIDACIONES INICIALES ==========
    if (pid < 0) {
        log_error(logger, "PID: %d - Error al crear proceso: Parámetros inválidos (tamaño=%d)", pid, tamanio);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    if (!nombre_archivo || strlen(nombre_archivo) == 0) {
        log_error(logger, "PID: %d - Error al crear proceso: Nombre de archivo inválido", pid);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    if (!sistema_memoria) {
        log_error(logger, "PID: %d - Error al crear proceso: Sistema de memoria no inicializado", pid);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }

    // Verificar si el proceso ya existe
    if (obtener_proceso(pid)) {
        log_error(logger, "PID: %d - Error al crear proceso: Ya existe", pid);
        return MEMORIA_ERROR_PROCESO_EXISTENTE;
    }

    // ========== CÁLCULO DE MEMORIA NECESARIA ==========
    int paginas_necesarias = (tamanio + cfg->TAM_PAGINA - 1) / cfg->TAM_PAGINA;
    log_debug(logger, "PID: %d - Páginas necesarias: %d (tamaño=%d, tam_pagina=%d)", 
              pid, paginas_necesarias, tamanio, cfg->TAM_PAGINA);

    // ========== VALIDACIÓN DE MEMORIA DISPONIBLE ==========
    pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);
    int marcos_disponibles = sistema_memoria->admin_marcos->frames_libres;
    
    if (marcos_disponibles < paginas_necesarias) {
        log_error(logger, "PID: %d - No hay suficiente memoria física (necesita %d páginas, disponibles %d)", 
                  pid, paginas_necesarias, marcos_disponibles);
        pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
        return MEMORIA_ERROR_NO_ESPACIO;
    }
    pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);

    // ========== CREACIÓN DE ESTRUCTURA DE PROCESO ==========
    t_proceso_memoria* proceso = crear_proceso_memoria(pid, tamanio);
    if (!proceso) {
        log_error(logger, "PID: %d - Error al crear estructura de proceso", pid);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }

    // Asignar nombre de archivo si fue proporcionado
    if (nombre_archivo) {
        proceso->nombre_archivo = strdup(nombre_archivo);
        if (!proceso->nombre_archivo) {
            log_error(logger, "PID: %d - Error al copiar nombre de archivo", pid);
            destruir_proceso(proceso);
            return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
        }
    }

    // ========== REGISTRO EN DICCIONARIOS DEL SISTEMA (ANTES DE ASIGNAR MARCOS) ==========
    char pid_str[16];
    sprintf(pid_str, "%d", pid);

    pthread_mutex_lock(&sistema_memoria->mutex_procesos);

    // Registrar en todos los diccionarios correspondientes
    dictionary_put(sistema_memoria->procesos, pid_str, proceso);
    dictionary_put(sistema_memoria->estructuras_paginas, pid_str, proceso->estructura_paginas);
    dictionary_put(sistema_memoria->metricas_procesos, pid_str, proceso->metricas);

    // ========== ACTUALIZACIÓN DE ESTADÍSTICAS DEL SISTEMA ==========
    sistema_memoria->procesos_activos++;
    sistema_memoria->memoria_utilizada += tamanio;
    sistema_memoria->total_asignaciones_memoria++;

    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);

    // ========== ASIGNACIÓN DE MARCOS FÍSICOS PARA TODAS LAS PÁGINAS ==========
    log_debug(logger, "PID: %d - Iniciando asignación de %d marcos físicos", pid, paginas_necesarias);
    
    t_resultado_memoria resultado_asignacion = asignar_marcos_proceso(pid);
    if (resultado_asignacion != MEMORIA_OK) {
        log_error(logger, "PID: %d - Error en asignación de marcos: %d", pid, resultado_asignacion);
        
        // Revertir el registro en diccionarios antes de destruir el proceso
        pthread_mutex_lock(&sistema_memoria->mutex_procesos);
        dictionary_remove(sistema_memoria->procesos, pid_str);
        dictionary_remove(sistema_memoria->estructuras_paginas, pid_str);
        dictionary_remove(sistema_memoria->metricas_procesos, pid_str);
        sistema_memoria->procesos_activos--;
        sistema_memoria->memoria_utilizada -= tamanio;
        sistema_memoria->total_asignaciones_memoria--;
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        
        destruir_proceso(proceso);
        return resultado_asignacion;
    }

    // ========== LOG OBLIGATORIO DE CREACIÓN ==========
    log_info(logger, VERDE("## (PID: %d) - Proceso Creado - Tamaño: %d"), pid, tamanio);
    
    log_debug(logger, "PID: %d - Proceso creado exitosamente:", pid);
    log_debug(logger, "   - Páginas totales: %d", paginas_necesarias);
    log_debug(logger, "   - Páginas asignadas: %d", proceso->estructura_paginas->paginas_asignadas);
    log_debug(logger, "   - Niveles de paginación: %d", cfg->CANTIDAD_NIVELES);
    log_debug(logger, "   - Marcos físicos utilizados: %d", paginas_necesarias);
    
    return MEMORIA_OK;
}

void liberar_proceso_memoria(t_proceso_memoria* proceso) {
    if (!proceso || !proceso->estructura_paginas) {
        return;
    }

    // Liberar frames asociados al proceso
    liberar_marcos_proceso(proceso->pid);

    // Eliminar proceso del diccionario
    char pid_str[16];
    sprintf(pid_str, "%d", proceso->pid);
    dictionary_remove(sistema_memoria->procesos, pid_str);
    destruir_proceso(proceso);

    log_trace(logger, "PID: %d - Proceso eliminado exitosamente", proceso->pid);
}

void destruir_proceso(t_proceso_memoria* proceso) {
    if (!proceso) return;

    if (proceso->estructura_paginas) {
        destruir_tabla_paginas_recursiva(proceso->estructura_paginas->tabla_raiz);
        free(proceso->estructura_paginas);
        proceso->estructura_paginas = NULL;
    }

    if (proceso->metricas) {
        destruir_metricas_proceso(proceso->metricas);
        proceso->metricas = NULL;
    }

    if (proceso->nombre_archivo) {
        free(proceso->nombre_archivo);
        proceso->nombre_archivo = NULL;
    }

    if (proceso->instrucciones) {
        list_destroy_and_destroy_elements(proceso->instrucciones, free);
        proceso->instrucciones = NULL;
    }

    free(proceso);
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
    log_debug(logger, "FINALIZAR_PROC: Imprimiendo métricas para PID %d", pid);
    imprimir_metricas_proceso(pid);
    log_debug(logger, "FINALIZAR_PROC: Métricas impresas para PID %d", pid);
    
    // Liberar todos los marcos físicos del proceso
    log_debug(logger, "FINALIZAR_PROC: Liberando marcos para PID %d", pid);
    liberar_marcos_proceso(pid);
    log_debug(logger, "FINALIZAR_PROC: Marcos liberados para PID %d", pid);
    
    // Actualizar estadísticas del sistema
    log_debug(logger, "FINALIZAR_PROC: Actualizando estadísticas del sistema para PID %d", pid);
    sistema_memoria->procesos_activos--;
    sistema_memoria->memoria_utilizada -= proceso->tamanio;
    sistema_memoria->total_liberaciones_memoria++;
    
    // Remover del diccionario y liberar
    log_debug(logger, "FINALIZAR_PROC: Removiendo de diccionarios para PID %d", pid);
    dictionary_remove(sistema_memoria->procesos, pid_str);
    dictionary_remove(sistema_memoria->estructuras_paginas, pid_str);
    dictionary_remove(sistema_memoria->metricas_procesos, pid_str);
    dictionary_remove(sistema_memoria->process_instructions, pid_str);
    
    log_debug(logger, "FINALIZAR_PROC: Liberando proceso para PID %d", pid);
    destruir_proceso(proceso);
    
    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
    
    log_trace(logger, "## PID: %d - Finaliza el proceso", pid);
    log_debug(logger, "FINALIZAR_PROC: Retornando MEMORIA_OK para PID %d", pid);
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
    
    // Escribir páginas del proceso a SWAP, liberar marcos y marcar como no presente
    int resultado = suspender_proceso_completo(pid);
    if (resultado != 1) {
        log_error(logger, "PID: %d - Error al suspender proceso a SWAP", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_IO;
    }
    
    // Actualizar estadísticas globales
    proceso->suspendido = true;
    proceso->activo = false;
    sistema_memoria->procesos_activos--;
    sistema_memoria->procesos_suspendidos++;
    sistema_memoria->memoria_utilizada -= proceso->tamanio;
    sistema_memoria->total_suspensiones++;
    
    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
    
    log_trace(logger, "## PID: %d - Proceso suspendido exitosamente", pid);
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

    // Lógica centralizada en manejo_swap.c
    int resultado = reanudar_proceso_suspendido(pid);
    if (resultado == 1) {
        // Actualizar campos y métricas del proceso
        proceso->suspendido = false;
        proceso->activo = true;
        sistema_memoria->procesos_activos++;
        sistema_memoria->procesos_suspendidos--;
        sistema_memoria->memoria_utilizada += proceso->tamanio;
        sistema_memoria->total_reanudaciones++;
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        log_info(logger, "PID: %d - Proceso reanudado exitosamente (vía manejo_swap)", pid);
        return MEMORIA_OK;
    } else {
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        log_error(logger, "PID: %d - Error al reanudar proceso (vía manejo_swap)", pid);
        return MEMORIA_ERROR_IO;
    }
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
    
    // CAMBIO: Inicializar estructura con memset para limpiar valores de basura
    memset(estructura, 0, sizeof(t_estructura_paginas));
    
    // Inicializar estructura
    estructura->pid = pid;
    estructura->cantidad_niveles = cfg->CANTIDAD_NIVELES;
    estructura->entradas_por_tabla = cfg->ENTRADAS_POR_TABLA;
    estructura->tam_pagina = cfg->TAM_PAGINA;
    estructura->tamanio_proceso = tamanio;
    estructura->paginas_totales = (tamanio + cfg->TAM_PAGINA - 1) / cfg->TAM_PAGINA;
    estructura->paginas_asignadas = 0;
    
    // CAMBIO: Inicializar explícitamente campos críticos
    estructura->paginas_asignadas = 0;          // Inicializar en 0
    estructura->paginas_en_swap = 0;            // Inicializar páginas en swap
    estructura->activo = true;                  // Proceso activo por defecto
    estructura->suspendido = false;             // No suspendido por defecto
    
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
    
    log_trace(logger, "PID: %d - Estructura de paginación creada - %d páginas, %d niveles", 
              pid, estructura->paginas_totales, estructura->cantidad_niveles);
    return estructura;
}

t_resultado_memoria configurar_entrada_pagina(t_estructura_paginas* estructura, int numero_pagina, int numero_frame) {
    if (!estructura) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    t_entrada_tabla* entrada = crear_entrada_tabla_si_no_existe(estructura, numero_pagina);
    if (!entrada) {
        log_error(logger, "PID: %d - Error al crear entrada para página %d", estructura->pid, numero_pagina);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    entrada->presente = true;
    entrada->numero_frame = numero_frame;
    entrada->modificado = false;
    entrada->referenciado = true;

    return MEMORIA_OK;
}

// ============================================================================
// FUNCIONES DE ACCESO A TABLAS DE PÁGINAS
// ============================================================================

/**
 * 1. ACCESO A TABLA DE PÁGINAS
 * El módulo deberá responder con el número de marco correspondiente. 
 * En este evento se deberá tener en cuenta la cantidad de niveles de tablas 
 * de páginas accedido, debiendo considerar un acceso (con su respectivo conteo 
 * de métricas y retardo de acceso) por cada nivel de tabla de páginas accedido.
 */
int acceso_tabla_paginas(int pid, int numero_pagina) {
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso || !proceso->estructura_paginas) {
        log_error(logger, "PID: %d - Estructura de páginas no encontrada", pid);
        return -1;
    }

    t_estructura_paginas* estructura = proceso->estructura_paginas;
    
    pthread_mutex_lock(&estructura->mutex_estructura);
    
    // Calcular índices para navegación multinivel
    int indices[estructura->cantidad_niveles];
    calcular_indices_multinivel(numero_pagina, estructura->cantidad_niveles, 
                             estructura->entradas_por_tabla, indices);
    
    // Navegar nivel por nivel desde la raíz hasta la hoja
    t_tabla_paginas* tabla_actual = estructura->tabla_raiz;
    
    for (int nivel = 0; nivel < estructura->cantidad_niveles; nivel++) {
        // APLICAR RETARDO POR CADA NIVEL ACCEDIDO
        aplicar_retardo_memoria();
        
        // INCREMENTAR MÉTRICA POR CADA NIVEL ACCEDIDO  
        incrementar_accesos_tabla_paginas(pid);
        
        int indice = indices[nivel];
        
        // Verificar que el índice esté dentro del rango
        if (indice >= estructura->entradas_por_tabla) {
            log_error(logger, "PID: %d - Índice fuera de rango en nivel %d", pid, nivel);
            pthread_mutex_unlock(&estructura->mutex_estructura);
            return -1;
        }
        
        t_entrada_tabla* entrada = &tabla_actual->entradas[indice];
        
        // Verificar que la entrada esté presente
        if (!entrada->presente) {
            log_error(logger, "PID: %d - Página %d no presente en nivel %d", pid, numero_pagina, nivel);
            pthread_mutex_unlock(&estructura->mutex_estructura);
            return -1;
        }
        
        // Si no estamos en el último nivel, navegar al siguiente
        if (nivel < estructura->cantidad_niveles - 1) {
            tabla_actual = entrada->tabla_siguiente;
            if (!tabla_actual) {
                log_error(logger, "PID: %d - Tabla siguiente nula en nivel %d", pid, nivel);
                pthread_mutex_unlock(&estructura->mutex_estructura);
                return -1;
            }
        } else {
            // ÚLTIMO NIVEL: Retornar el número de frame
            int numero_frame = entrada->numero_frame;
            pthread_mutex_unlock(&estructura->mutex_estructura);
            
            log_trace(logger, "## PID: %d - ACCESO TABLA PÁGINAS - Página: %d - Marco: %d - Niveles accedidos: %d", 
                     pid, numero_pagina, numero_frame, estructura->cantidad_niveles);
            return numero_frame;
        }
    }
    
    // No debería llegar aca
    pthread_mutex_unlock(&estructura->mutex_estructura);
    return -1;
}

// ============================================================================
// FUNCIONES DE ACCESO A MEMORIA FÍSICA
// ============================================================================

/**
 * LECTURA EN MEMORIA FÍSICA
 * Lee datos desde una dirección física específica
 */
t_resultado_memoria leer_memoria_fisica(uint32_t direccion_fisica, int tamanio, void* buffer) {
    if (!sistema_memoria || !buffer) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Validar que la dirección física está dentro del rango de memoria
    if (direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "Dirección física fuera de rango: %u", direccion_fisica);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Copiar datos desde la memoria física al buffer
    memcpy(buffer, (char*)sistema_memoria->memoria_principal + direccion_fisica, tamanio);
    
    return MEMORIA_OK;
}

t_resultado_memoria escribir_memoria_fisica(uint32_t direccion_fisica, void* datos, int tamanio) {
    if (!sistema_memoria || !datos) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Validar que la dirección física está dentro del rango de memoria
    if (direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "Dirección física fuera de rango: %u", direccion_fisica);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Copiar datos desde el buffer a la memoria física
    memcpy((char*)sistema_memoria->memoria_principal + direccion_fisica, datos, tamanio);
    
    return MEMORIA_OK;
}

// ============================================================================
// FUNCIONES DE UTILIDAD
// ============================================================================

/**
 * @brief Libera un marco sin adquirir el mutex (versión interna)
 * @param numero_frame Número del marco a liberar
 * @return Resultado de la operación
 */
static t_resultado_memoria liberar_marco_interno(int numero_frame) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        log_error(logger, "Sistema de memoria no inicializado");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    t_administrador_marcos* admin = sistema_memoria->admin_marcos;
    
    if (numero_frame < 0 || numero_frame >= admin->cantidad_total_frames) {
        log_error(logger, "Número de frame inválido: %d", numero_frame);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    t_frame* frame = &admin->frames[numero_frame];
    
    if (!frame->ocupado) {
        log_warning(logger, "Intento de liberar frame ya libre: %d", numero_frame);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    int pid_anterior = frame->pid_propietario;
    int pagina_anterior = frame->numero_pagina;
    
    // Limpiar el frame
    frame->ocupado = false;
    frame->pid_propietario = -1;
    frame->numero_pagina = -1;
    frame->timestamp_asignacion = 0;
    
    // Limpiar contenido del frame (opcional, para debugging)
    memset(frame->contenido, 0, sistema_memoria->tam_pagina);
    
    // Actualizar bitmap (marcar como libre)
    bitarray_clean_bit(admin->bitmap_frames, numero_frame);
    
    // Agregar a lista de frames libres
    int* frame_num = malloc(sizeof(int));
    *frame_num = numero_frame;
    list_add(admin->lista_frames_libres, frame_num);
    
    // Actualizar contadores
    admin->frames_libres++;
    admin->frames_ocupados--;
    admin->total_liberaciones++;
    
    log_trace(logger, "## Marco liberado - Frame: %d (era PID: %d, Página: %d)", numero_frame, pid_anterior, pagina_anterior);
    return MEMORIA_OK;
}

static void liberar_marcos_proceso(int pid) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        log_error(logger, "liberar_marcos_proceso: Sistema de memoria no inicializado");
        return;
    }

    log_debug(logger, "LIBERAR_MARCOS: Iniciando liberación de marcos para PID %d", pid);
    pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);
    log_debug(logger, "LIBERAR_MARCOS: Mutex adquirido para PID %d", pid);

    int marcos_liberados = 0;
    for (int i = 0; i < sistema_memoria->admin_marcos->cantidad_total_frames; i++) {
        t_frame* frame = &sistema_memoria->admin_marcos->frames[i];
        if (frame->ocupado && frame->pid_propietario == pid) {
            log_debug(logger, "LIBERAR_MARCOS: Liberando marco %d para PID %d", i, pid);
            liberar_marco_interno(i);
            marcos_liberados++;
            log_debug(logger, "LIBERAR_MARCOS: Marco %d liberado para PID %d", i, pid);
        }
    }

    log_debug(logger, "LIBERAR_MARCOS: Total de marcos liberados para PID %d: %d", pid, marcos_liberados);
    pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
    log_debug(logger, "LIBERAR_MARCOS: Mutex liberado para PID %d", pid);
}

void* acceso_espacio_usuario_lectura(int pid, int direccion_fisica, int tamanio) {
    log_info(logger, VERDE("(PID: %d) - Lectura - Dir. Física: %d - Tamaño: %d"), 
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
    
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para lectura", pid);
        return NULL;
    }
    
    t_list* paginas_bloqueadas = NULL;
    
    // Proteger página con bloqueo para lectura
    if (bloquear_marco_por_pagina(pid, direccion_fisica / cfg->TAM_PAGINA, "LECTURA_ESPACIO_USUARIO")) {
        paginas_bloqueadas = list_create();
        int* pag = malloc(sizeof(int));
        *pag = direccion_fisica / cfg->TAM_PAGINA;
        list_add(paginas_bloqueadas, pag);
    } else {
        log_warning(logger, "PID: %d - No se pudo bloquear página %d para lectura", pid, direccion_fisica / cfg->TAM_PAGINA);
        return NULL;
    }
    
    // Obtener el número de marco físico correspondiente a la página
    int numero_pagina = direccion_fisica / cfg->TAM_PAGINA;
    int numero_marco = acceso_tabla_paginas(pid, numero_pagina);
    if (numero_marco == -1) {
        int* pag = list_get(paginas_bloqueadas, 0);
        desbloquear_marco_por_pagina(pid, *pag, "LECTURA_ESPACIO_USUARIO");
        list_destroy_and_destroy_elements(paginas_bloqueadas, free);
        log_error(logger, "PID: %d - No se pudo obtener marco para página %d", pid, numero_pagina);
        return NULL;
    }
    
    // Leer el contenido completo del marco
    void* dato_leido = malloc(cfg->TAM_PAGINA);
    if (!dato_leido) {
        int* pag = list_get(paginas_bloqueadas, 0);
        desbloquear_marco_por_pagina(pid, *pag, "LECTURA_ESPACIO_USUARIO");
        list_destroy_and_destroy_elements(paginas_bloqueadas, free);
        log_error(logger, "Error al asignar memoria para leer marco %d", numero_marco);
        return NULL;
    }
    
    if (!leer_contenido_marco(numero_marco, dato_leido)) {
        free(dato_leido);
        int* pag = list_get(paginas_bloqueadas, 0);
        desbloquear_marco_por_pagina(pid, *pag, "LECTURA_ESPACIO_USUARIO");
        list_destroy_and_destroy_elements(paginas_bloqueadas, free);
        log_error(logger, "PID: %d - Error al leer contenido del marco %d", pid, numero_marco);
        return NULL;
    }
    
    // Incrementar métrica de lecturas de memoria
    incrementar_lecturas_memoria(pid);
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Obtener datos específicos con offset dentro de la página
    int offset_en_pagina = direccion_fisica % cfg->TAM_PAGINA;
    char* valor_leido = malloc(tamanio + 1);
    if (!valor_leido) {
        free(dato_leido);
        int* pag = list_get(paginas_bloqueadas, 0);
        desbloquear_marco_por_pagina(pid, *pag, "LECTURA_ESPACIO_USUARIO");
        list_destroy_and_destroy_elements(paginas_bloqueadas, free);
        log_error(logger, "Error al asignar memoria para valor leído");
        return NULL;
    }
    
    memcpy(valor_leido, (char*)dato_leido + offset_en_pagina, tamanio);
    valor_leido[tamanio] = '\0';
    
    // Liberar recursos
    free(dato_leido);
    int* pag = list_get(paginas_bloqueadas, 0);
    desbloquear_marco_por_pagina(pid, *pag, "LECTURA_ESPACIO_USUARIO");
    list_destroy_and_destroy_elements(paginas_bloqueadas, free);
    
    log_info(logger, VERDE("## (PID: %d) - Lectura - Dir. Física: %d - Tamaño: %d"), 
             pid, direccion_fisica, tamanio);
    
    return valor_leido;
}

/**
 * 2. ACCESO A ESPACIO DE USUARIO - ESCRITURA
 * Ante un pedido de escritura, escribir lo indicado en la posición pedida. 
 * En caso satisfactorio se responderá un mensaje de 'OK'.
 */
bool acceso_espacio_usuario_escritura(int pid, int direccion_fisica, int tamanio, void* datos) {
    log_info(logger, VERDE("(PID: %d) - Escritura - Dir. Física: %d - Tamaño: %d"), 
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
    
    // Variable para mantener páginas bloqueadas
    t_list* paginas_bloqueadas = NULL;
    
    // Proteger página con bloqueo para escritura
    if (bloquear_marco_por_pagina(pid, direccion_fisica / cfg->TAM_PAGINA, "ESCRITURA_ESPACIO_USUARIO")) {
        paginas_bloqueadas = list_create();
        int* pag = malloc(sizeof(int));
        *pag = direccion_fisica / cfg->TAM_PAGINA;
        list_add(paginas_bloqueadas, pag);
    } else {
        log_warning(logger, "PID: %d - No se pudo bloquear página %d para escritura", pid, direccion_fisica / cfg->TAM_PAGINA);
        return false;
    }
    
    // Escribir datos en el espacio de usuario
    memcpy(sistema_memoria->memoria_principal + direccion_fisica, datos, tamanio);
    
    // MARCAR PÁGINA COMO MODIFICADA (DIRTY BIT)
    t_entrada_tabla* entrada = _buscar_entrada_pagina(pid, direccion_fisica / cfg->TAM_PAGINA);
    if (entrada) {
        entrada->modificado = true;
        log_trace(logger, "PID: %d - Página %d marcada como modificada (dirty bit)", pid, direccion_fisica / cfg->TAM_PAGINA);
    }
    
    // Liberar recursos
    int* pag = list_get(paginas_bloqueadas, 0);
    desbloquear_marco_por_pagina(pid, *pag, "ESCRITURA_ESPACIO_USUARIO");
    list_destroy_and_destroy_elements(paginas_bloqueadas, free);
    
    log_info(logger, VERDE("## (PID: %d) - Escritura - Dir. Física: %d - Tamaño: %d"), 
             pid, direccion_fisica, tamanio);
    
    return true;
}

/**
 * 3. LEER PÁGINA COMPLETA
 * Se deberá devolver el contenido correspondiente de la página a partir del byte 
 * enviado como dirección física dentro de la Memoria de Usuario, que deberá 
 * coincidir con la posición del byte 0 de la página.
 */
void* leer_pagina_completa(int pid, int direccion_base_pagina) {
    log_info(logger, "PID: %d - Leer página completa - Dir. Base Página: %d", pid, direccion_base_pagina);
    
    // Validar que la dirección coincide con el byte 0 de una página
    if (direccion_base_pagina % cfg->TAM_PAGINA != 0) {
        log_error(logger, "PID: %d - Dirección base %d no coincide con byte 0 de página (TAM_PAGINA=%d)", 
                 pid, direccion_base_pagina, cfg->TAM_PAGINA);
        return NULL;
    }
    
    // Validar que la dirección está dentro del espacio de memoria
    if (direccion_base_pagina + cfg->TAM_PAGINA > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Página fuera de rango: %d + %d > %d", 
                 pid, direccion_base_pagina, cfg->TAM_PAGINA, cfg->TAM_MEMORIA);
        return NULL;
    }
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para leer página completa", pid);
        return NULL;
    }
    
    t_list* paginas_bloqueadas = NULL;
    
    // Calcular el número de marco desde la dirección base física
    int numero_marco = direccion_base_pagina / cfg->TAM_PAGINA;
    
    // Obtener el número de página que está mapeada a este marco
    int numero_pagina = obtener_numero_pagina_de_marco(pid, numero_marco);
    if (numero_pagina == -1) {
        log_error(logger, "PID: %d - No se encontró página mapeada al marco %d", pid, numero_marco);
        return NULL;
    }
    
    // Proteger página con bloqueo para lectura
    if (bloquear_marco_por_pagina(pid, numero_pagina, "LECTURA_PAGINA_COMPLETA")) {
        paginas_bloqueadas = list_create();
        int* pag = malloc(sizeof(int));
        *pag = numero_pagina;
        list_add(paginas_bloqueadas, pag);
    } else {
        log_error(logger, "PID: %d - No se pudo bloquear página %d para lectura completa", pid, numero_pagina);
        return NULL;
    }
    
    // Incrementar métrica de lecturas usando función estándar
    incrementar_lecturas_memoria(pid);
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Reservar memoria para la página completa
    void* pagina_completa = malloc(cfg->TAM_PAGINA);
    if (pagina_completa == NULL) {
        log_error(logger, "PID: %d - Error al reservar memoria para página completa", pid);
        int* pag = list_get(paginas_bloqueadas, 0);
        desbloquear_marco_por_pagina(pid, *pag, "LECTURA_PAGINA_COMPLETA");
        list_destroy_and_destroy_elements(paginas_bloqueadas, free);
        return NULL;
    }
    
    // Copiar página completa desde el espacio de usuario
    memcpy(pagina_completa, sistema_memoria->memoria_principal + direccion_base_pagina, cfg->TAM_PAGINA);
    
    // Liberar recursos
    int* pag = list_get(paginas_bloqueadas, 0);
    desbloquear_marco_por_pagina(pid, *pag, "LECTURA_PAGINA_COMPLETA");
    list_destroy_and_destroy_elements(paginas_bloqueadas, free);
    
    log_trace(logger, "PID: %d - Página completa leída desde dirección base %d", pid, direccion_base_pagina);
    
    return pagina_completa;
}

/**
 * 4. ACTUALIZAR PÁGINA COMPLETA
 * Se escribirá la página completa a partir del byte 0 que igual será enviado 
 * como dirección física, esta operación se realizará dentro de la Memoria de 
 * Usuario y se responderá como OK.
 */
bool actualizar_pagina_completa(int pid, int direccion_fisica, void* contenido_pagina) {
    log_info(logger, "(PID: %d) - Actualizar página completa - Dir. Física: %d", pid, direccion_fisica);
    
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
    
    t_list* paginas_bloqueadas = NULL;
    
    // Calcular el número de marco desde la dirección física
    int numero_marco = direccion_fisica / cfg->TAM_PAGINA;
    
    // Obtener el número de página que está mapeada a este marco
    int numero_pagina = obtener_numero_pagina_de_marco(pid, numero_marco);
    if (numero_pagina == -1) {
        log_error(logger, "PID: %d - No se encontró página mapeada al marco %d", pid, numero_marco);
        return false;
    }
    
    // Proteger página con bloqueo para escritura
    if (bloquear_marco_por_pagina(pid, numero_pagina, "ESCRITURA_PAGINA_COMPLETA")) {
        paginas_bloqueadas = list_create();
        int* pag = malloc(sizeof(int));
        *pag = numero_pagina;
        list_add(paginas_bloqueadas, pag);
    } else {
        log_error(logger, "PID: %d - No se pudo bloquear página %d para escritura completa", pid, numero_pagina);
        return false;
    }
    
    // Incrementar métrica de escrituras usando función estándar
    incrementar_escrituras_memoria(pid);
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Escribir página completa en el espacio de usuario
    memcpy(sistema_memoria->memoria_principal + direccion_fisica, contenido_pagina, cfg->TAM_PAGINA);
    
    // MARCAR LA PÁGINA COMO MODIFICADA (DIRTY BIT)
    t_entrada_tabla* entrada = _buscar_entrada_pagina(pid, numero_pagina);
    if (entrada) {
        entrada->modificado = true;
        log_trace(logger, "PID: %d - Página %d marcada como modificada (dirty bit)", pid, numero_pagina);
    }
    
    // Liberar recursos
    int* pag = list_get(paginas_bloqueadas, 0);
    desbloquear_marco_por_pagina(pid, *pag, "ESCRITURA_PAGINA_COMPLETA");
    list_destroy_and_destroy_elements(paginas_bloqueadas, free);
    
    log_trace(logger, "PID: %d - Página completa actualizada en dirección física %d", pid, direccion_fisica);
    
    return true;
}

// ============================================================================
// FUNCIONES AUXILIARES DE ACCESO A MEMORIA FÍSICA
// ============================================================================

t_resultado_memoria leer_pagina_memoria(int numero_frame, void* buffer) {
    if (!sistema_memoria || !buffer) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Calcular dirección física del frame
    uint32_t direccion_fisica = numero_frame * cfg->TAM_PAGINA;

    // Leer la página completa
    return leer_memoria_fisica(direccion_fisica, cfg->TAM_PAGINA, buffer);
}

t_resultado_memoria escribir_pagina_memoria(int numero_frame, void* contenido) {
    if (!sistema_memoria || !contenido) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Calcular dirección física del frame
    uint32_t direccion_fisica = numero_frame * cfg->TAM_PAGINA;

    // Escribir la página completa
    return escribir_memoria_fisica(direccion_fisica, contenido, cfg->TAM_PAGINA);
}

t_entrada_tabla* crear_entrada_tabla_si_no_existe(t_estructura_paginas* estructura, int numero_pagina) {
    if (!estructura || !estructura->tabla_raiz) {
        return NULL;
    }

    int indices[estructura->cantidad_niveles];
    calcular_indices_multinivel(numero_pagina, estructura->cantidad_niveles, 
                             estructura->entradas_por_tabla, indices);

    t_tabla_paginas* tabla_actual = estructura->tabla_raiz;
    t_entrada_tabla* entrada = NULL;

    for (int nivel = 0; nivel < estructura->cantidad_niveles; nivel++) {
        if (!tabla_actual || indices[nivel] >= estructura->entradas_por_tabla) {
            return NULL;
        }

        entrada = &tabla_actual->entradas[indices[nivel]];
        
        if (nivel < estructura->cantidad_niveles - 1) {
            if (!entrada->presente) {
                t_tabla_paginas* nueva_tabla = crear_tabla_paginas(nivel + 1);
                if (!nueva_tabla) {
                    return NULL;
                }
                entrada->presente = true;
                entrada->tabla_siguiente = nueva_tabla;
            }
            tabla_actual = entrada->tabla_siguiente;
        }
    }

    return entrada;
}

/**
 * Función inversa de calcular_indices_multinivel
 * Calcula el número de página a partir de las entradas de cada nivel
 */
int calcular_numero_pagina_desde_entradas(int* entradas, int cantidad_niveles, int entradas_por_tabla) {
    int numero_pagina = 0;
    
    for (int nivel = 0; nivel < cantidad_niveles; nivel++) {
        int potencia = 1;
        for (int j = 0; j < cantidad_niveles - (nivel + 1); j++) {
            potencia *= entradas_por_tabla;
        }
        numero_pagina += entradas[nivel] * potencia;
    }
    
    return numero_pagina;
}


