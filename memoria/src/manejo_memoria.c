#include "../headers/manejo_memoria.h"
#include "../headers/interfaz_memoria.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/metricas.h"
#include "../headers/manejo_swap.h"
#include "../headers/utils.h"
#include "../headers/monitor_memoria.h"
#include <commons/log.h>
#include <commons/string.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Variables externas
extern t_sistema_memoria* sistema_memoria;
extern t_log* logger;
extern t_config_memoria* cfg;

// Declaraciones de funciones internas
static void liberar_marcos_proceso(int pid);

// ============================================================================
// FUNCIONES DE GESTIÓN DE PROCESOS EN MEMORIA
// ============================================================================

/**
 * Crea la estructura básica de un proceso en memoria sin asignar marcos físicos
 * Esta función crea todas las estructuras administrativas necesarias
 */
t_proceso_memoria* crear_proceso_memoria(int pid, int tamanio) {
    if (pid < 0 || tamanio <= 0) {
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

    return proceso;
}

t_resultado_memoria crear_proceso_en_memoria(int pid, int tamanio, char* nombre_archivo) {
    // ========== VALIDACIONES INICIALES ==========
    if (pid < 0 || tamanio <= 0) {
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
    log_info(logger, "## PID: %d - Proceso Creado - Tamaño: %d", pid, tamanio);
    
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
    }

    if (proceso->metricas) {
        destruir_metricas_proceso(proceso->metricas);
    }

    if (proceso->nombre_archivo) {
        free(proceso->nombre_archivo);
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
    imprimir_metricas_proceso(pid);
    
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
    
    log_trace(logger, "## PID: %d - Finaliza el proceso", pid);
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
    t_resultado_memoria resultado = suspender_proceso_completo(pid);
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
    
    // Verificar si hay suficientes marcos libres
    int paginas_necesarias = proceso->estructura_paginas->paginas_totales;
    if (obtener_marcos_libres() < paginas_necesarias) {
        log_error(logger, "PID: %d - Memoria insuficiente para reanudar. Necesarias: %d, Disponibles: %d", 
                  pid, paginas_necesarias, obtener_marcos_libres());
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Leer proceso desde SWAP
    t_resultado_memoria resultado = reanudar_proceso_suspendido(pid);
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
    
    log_trace(logger, "## PID: %d - Proceso reanudado exitosamente", pid);
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
    calcular_indices_multinivel(numero_pagina, estructura->entradas_por_tabla, 
                             estructura->cantidad_niveles, indices);
    
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

static void liberar_marcos_proceso(int pid) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        log_error(logger, "liberar_marcos_proceso: Sistema de memoria no inicializado");
        return;
    }

    pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);

    for (int i = 0; i < sistema_memoria->admin_marcos->cantidad_total_frames; i++) {
        t_frame* frame = &sistema_memoria->admin_marcos->frames[i];
        if (frame->ocupado && frame->pid_propietario == pid) {
            liberar_marco(i);
        }
    }

    pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
}

void* acceso_espacio_usuario_lectura(int pid, int direccion_fisica, int tamanio) {
    log_info(logger, "PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", 
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
    
    // Incrementar métrica de lecturas usando función estándar
    incrementar_lecturas_memoria(pid);
    
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
    log_info(logger, "PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", 
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
    
    // Incrementar métrica de escrituras usando función estándar
    incrementar_escrituras_memoria(pid);
    
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
    log_info(logger, "PID: %d - Leer página completa - Dir. Física: %d", pid, direccion_fisica);
    
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
    
    // Incrementar métrica de lecturas usando función estándar
    incrementar_lecturas_memoria(pid);
    
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
    log_info(logger, "PID: %d - Actualizar página completa - Dir. Física: %d", pid, direccion_fisica);
    
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
    
    // Incrementar métrica de escrituras usando función estándar
    incrementar_escrituras_memoria(pid);
    
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
    calcular_indices_multinivel(numero_pagina, estructura->entradas_por_tabla, 
                             estructura->cantidad_niveles, indices);

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

/**
 * Procesa una solicitud de frame para entradas multinivel
 * Extrae los parámetros del paquete y calcula el marco correspondiente
 */
int procesar_solicitud_frame_entradas(t_list* lista) {
    if (!lista || list_size(lista) < 2) {
        log_error(logger, "Lista de parámetros inválida para solicitud de frame");
        return -1;
    }
    
    // CORREGIDO: recibir_contenido_paquete ya devuelve punteros a int
    int pid = *((int*)list_get(lista, 0));
    int cantidad_niveles = *((int*)list_get(lista, 1));
    
    if (list_size(lista) < 2 + cantidad_niveles) {
        log_error(logger, "PID: %d - Parámetros insuficientes para %d niveles (recibidos: %d)", 
                  pid, cantidad_niveles, list_size(lista));
        return -1;
    }
    
    log_trace(logger, "Procesando solicitud de frame para entradas - PID: %d, Niveles: %d", pid, cantidad_niveles);
    
    // Extraer las entradas de cada nivel
    int entradas[cantidad_niveles];
    for (int i = 0; i < cantidad_niveles; i++) {
        entradas[i] = *((int*)list_get(lista, i + 2));
        log_trace(logger, "  Entrada[%d] = %d", i, entradas[i]);
    }
    
    // Calcular número de página usando función dedicada
    int numero_pagina = calcular_numero_pagina_desde_entradas(entradas, cantidad_niveles, cfg->ENTRADAS_POR_TABLA);
    
    log_trace(logger, "Página calculada: %d para PID: %d", numero_pagina, pid);
    
    // Realizar acceso a tabla de páginas usando función existente
    return acceso_tabla_paginas(pid, numero_pagina);
}


