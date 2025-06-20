#include "../headers/manejo_memoria.h"
#include "../headers/interfaz_memoria.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/metricas.h"
#include "../headers/manejo_swap.h"
#include "../headers/utils.h"
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
static t_resultado_memoria configurar_entrada_pagina(t_estructura_paginas* estructura, int numero_pagina, int numero_frame);
static void calcular_indices_multinivel(int numero_pagina, int cantidad_niveles, int entradas_por_tabla, int* indices);

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

        // ========== REGISTRO EN DICCIONARIOS DEL SISTEMA ==========
        char pid_str[16];
        sprintf(pid_str, "%d", pid);
    
        pthread_mutex_lock(&sistema_memoria->mutex_procesos);
    
        // Registrar en todos los diccionarios correspondientes
        dictionary_put(sistema_memoria->procesos, pid_str, proceso);
        dictionary_put(sistema_memoria->estructuras_paginas, pid_str, proceso->estructura_paginas);
        dictionary_put(sistema_memoria->metricas_procesos, pid_str, proceso->metricas);
        
    // ========== ASIGNACIÓN DE MARCOS FÍSICOS PARA TODAS LAS PÁGINAS ==========
    log_debug(logger, "PID: %d - Iniciando asignación de %d marcos físicos", pid, paginas_necesarias);
    
    t_resultado_memoria resultado_asignacion = asignar_marcos_proceso(pid);
    if (resultado_asignacion != MEMORIA_OK) {
        log_error(logger, "PID: %d - Error en asignación de marcos: %d", pid, resultado_asignacion);
        destruir_proceso(proceso);
        return resultado_asignacion;
    }

    // ========== ACTUALIZACIÓN DE ESTADÍSTICAS DEL SISTEMA ==========
    sistema_memoria->procesos_activos++;
    sistema_memoria->memoria_utilizada += tamanio;
    sistema_memoria->total_asignaciones_memoria++;

    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);

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
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso || !proceso->estructura_paginas) {
        return;
    }

    t_estructura_paginas* estructura = proceso->estructura_paginas;
    for (int i = 0; i < estructura->paginas_totales; i++) {
        t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, i);
        if (entrada && entrada->presente) {
            liberar_marco(entrada->numero_frame);
            entrada->presente = false;
            entrada->numero_frame = 0;
        }
    }
}

void aplicar_retardo_memoria(void) {
    if (cfg->RETARDO_MEMORIA > 0) {
        usleep(cfg->RETARDO_MEMORIA * 1000); // Convertir ms a microsegundos
    }
}

void liberar_instruccion(t_instruccion* instruccion) {
    if (instruccion != NULL) {
        if (instruccion->parametros1 != NULL) free(instruccion->parametros1);
        if (instruccion->parametros2 != NULL) free(instruccion->parametros2);
        if (instruccion->parametros3 != NULL) free(instruccion->parametros3);
        free(instruccion);
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
 * 2. ACCESO A ESPACIO DE USUARIO - LECTURA
 * Ante un pedido de lectura, devolver el valor que se encuentra en la posición pedida.
 */
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

t_entrada_tabla* buscar_entrada_tabla(t_estructura_paginas* estructura, int numero_pagina) {
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
                return NULL;
            }
            tabla_actual = entrada->tabla_siguiente;
        }
    }

    return entrada;
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

int asignar_frame_libre(int pid, int numero_pagina) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        return -1;
    }

    pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);

    if (sistema_memoria->admin_marcos->frames_libres == 0) {
        pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
        return -1;
    }

    // Obtener el primer frame libre de la lista
    int numero_frame = *(int*)list_remove(sistema_memoria->admin_marcos->lista_frames_libres, 0);
    t_frame* frame = &sistema_memoria->admin_marcos->frames[numero_frame];

    // Actualizar el frame
    frame->ocupado = true;
    frame->pid_propietario = pid;
    frame->numero_pagina = numero_pagina;
    frame->timestamp_asignacion = time(NULL);

    // Actualizar contadores
    sistema_memoria->admin_marcos->frames_libres--;
    sistema_memoria->admin_marcos->frames_ocupados++;
    sistema_memoria->admin_marcos->total_asignaciones++;

    // Actualizar bitmap
    bitarray_set_bit(sistema_memoria->admin_marcos->bitmap_frames, numero_frame);

    pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);

    return numero_frame;
}

/**
 * @deprecated Usar liberar_marco en su lugar
 * Función wrapper para mantener compatibilidad
 */
void liberar_frame(int numero_frame) {
    log_warning(logger, "DEPRECATED: liberar_frame() - Usar liberar_marco() en su lugar");
    liberar_marco(numero_frame);
}

t_proceso_memoria* obtener_proceso(int pid) {
    if (!sistema_memoria || !sistema_memoria->procesos) {
        return NULL;
    }
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    return dictionary_get(sistema_memoria->procesos, pid_str);
}

t_metricas_proceso* crear_metricas_proceso(int pid) {
    t_metricas_proceso* metricas = malloc(sizeof(t_metricas_proceso));
    if (!metricas) {
        log_error(logger, "Error al crear métricas para proceso %d", pid);
        return NULL;
    }

    metricas->pid = pid;
    metricas->accesos_tabla_paginas = 0;
    metricas->instrucciones_solicitadas = 0;
    metricas->bajadas_swap = 0;
    metricas->subidas_memoria_principal = 0;
    metricas->lecturas_memoria = 0;
    metricas->escrituras_memoria = 0;
    metricas->timestamp_creacion = time(NULL);
    metricas->timestamp_ultimo_acceso = time(NULL);

    pthread_mutex_init(&metricas->mutex_metricas, NULL);

    return metricas;
}

static void calcular_indices_multinivel(int numero_pagina, int cantidad_niveles, int entradas_por_tabla, int* indices) {
    int pagina_temp = numero_pagina;
    for (int nivel = 0; nivel < cantidad_niveles; nivel++) {
        indices[nivel] = pagina_temp % entradas_por_tabla;
        pagina_temp /= entradas_por_tabla;
    }
}

t_tabla_paginas* crear_tabla_paginas(int nivel) {
    t_tabla_paginas* tabla = malloc(sizeof(t_tabla_paginas));
    if (!tabla) {
        log_error(logger, "Error al crear tabla de páginas nivel %d", nivel);
        return NULL;
    }
    
    tabla->nivel = nivel;
    tabla->entradas = calloc(cfg->ENTRADAS_POR_TABLA, sizeof(t_entrada_tabla));
    
    if (!tabla->entradas) {
        log_error(logger, "Error al asignar memoria para entradas de tabla nivel %d", nivel);
        free(tabla);
        return NULL;
    }
    
    return tabla;
}

/**
 * Asigna todos los marcos necesarios para un proceso según su tamaño
 * Esta función es utilizada durante la creación o des-suspensión de procesos
 */
t_resultado_memoria asignar_marcos_proceso(int pid) {
    log_trace(logger, "PID: %d - Iniciando asignación de marcos para proceso", pid);
    
    // ========== VALIDACIONES INICIALES ==========
    if (pid < 0) {
        log_error(logger, "PID: %d - Error: PID inválido para asignación de marcos", pid);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        log_error(logger, "PID: %d - Error: Sistema de memoria no inicializado", pid);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }

    // ========== OBTENER PROCESO EXISTENTE ==========
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso) {
        log_error(logger, "PID: %d - Error: Proceso no existe para asignar marcos", pid);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }

    if (!proceso->estructura_paginas) {
        log_error(logger, "PID: %d - Error: Estructura de páginas no inicializada", pid);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }

    // ========== CÁLCULO DE MARCOS NECESARIOS ==========
    int paginas_totales = proceso->estructura_paginas->paginas_totales;
    int paginas_ya_asignadas = proceso->estructura_paginas->paginas_asignadas;
    int paginas_necesarias = paginas_totales - paginas_ya_asignadas;

    if (paginas_necesarias <= 0) {
        log_debug(logger, "PID: %d - Todos los marcos ya están asignados (%d/%d)", 
                  pid, paginas_ya_asignadas, paginas_totales);
        return MEMORIA_OK;
    }

    log_debug(logger, "PID: %d - Marcos a asignar: %d (total=%d, asignadas=%d)", 
              pid, paginas_necesarias, paginas_totales, paginas_ya_asignadas);

    // ========== VERIFICAR MEMORIA DISPONIBLE ==========
    pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);
    int marcos_disponibles = sistema_memoria->admin_marcos->frames_libres;
    
    if (marcos_disponibles < paginas_necesarias) {
        log_error(logger, "PID: %d - No hay suficiente memoria física (necesita %d marcos, disponibles %d)", 
                  pid, paginas_necesarias, marcos_disponibles);
        pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
        return MEMORIA_ERROR_NO_ESPACIO;
    }
    pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);

    // ========== ASIGNACIÓN DE MARCOS ==========
    bool asignacion_exitosa = true;
    int marcos_asignados_en_esta_operacion = 0;

    for (int numero_pagina = 0; numero_pagina < paginas_totales && asignacion_exitosa; numero_pagina++) {
        // Verificar si la página ya tiene marco asignado
        t_entrada_tabla* entrada = buscar_entrada_tabla(proceso->estructura_paginas, numero_pagina);
        if (entrada && entrada->presente) {
            continue; // Esta página ya tiene marco asignado
        }

        // Asignar nuevo marco físico
        int numero_marco = asignar_marco_libre(pid, numero_pagina);
        if (numero_marco == -1) {
            log_error(logger, "PID: %d - Error al asignar marco para página %d", pid, numero_pagina);
            asignacion_exitosa = false;
            break;
        }
        marcos_asignados_en_esta_operacion++;

        // Crear entrada en la tabla de páginas si no existe
        if (!entrada) {
            entrada = crear_entrada_tabla_si_no_existe(proceso->estructura_paginas, numero_pagina);
            if (!entrada) {
                log_error(logger, "PID: %d - Error al crear entrada de tabla para página %d", pid, numero_pagina);
                liberar_marco(numero_marco);
                asignacion_exitosa = false;
                break;
            }
        }

        // Configurar la entrada de tabla
        entrada->presente = true;
        entrada->numero_frame = numero_marco;
        entrada->modificado = false;
        entrada->referenciado = true;
        entrada->timestamp_acceso = time(NULL);

        // Actualizar contador de páginas asignadas
        proceso->estructura_paginas->paginas_asignadas++;

        log_trace(logger, "PID: %d - Página %d asignada al marco %d", 
                  pid, numero_pagina, numero_marco);
    }

    // ========== MANEJO DE ERROR EN ASIGNACIÓN ==========
    if (!asignacion_exitosa) {
        log_error(logger, "PID: %d - Falló asignación de marcos, liberando %d marcos asignados en esta operación", 
                  pid, marcos_asignados_en_esta_operacion);
        
        // Liberar solo los marcos asignados en esta operación
        int marcos_liberados = 0;
        for (int numero_pagina = 0; numero_pagina < paginas_totales && marcos_liberados < marcos_asignados_en_esta_operacion; numero_pagina++) {
            t_entrada_tabla* entrada = buscar_entrada_tabla(proceso->estructura_paginas, numero_pagina);
            if (entrada && entrada->presente) {
                // Solo liberar si este marco fue asignado en esta operación
                // (verificamos el timestamp o simplemente liberamos los últimos asignados)
                t_frame* frame = obtener_frame(entrada->numero_frame);
                if (frame && frame->timestamp_asignacion >= time(NULL) - 1) { // Asignado en el último segundo
                    liberar_marco(entrada->numero_frame);
                    entrada->presente = false;
                    entrada->numero_frame = 0;
                    proceso->estructura_paginas->paginas_asignadas--;
                    marcos_liberados++;
                }
            }
        }

        return MEMORIA_ERROR_NO_ESPACIO;
    }

    // ========== ACTUALIZACIÓN DE MÉTRICAS ==========
    if (proceso->metricas) {
        pthread_mutex_lock(&proceso->metricas->mutex_metricas);
        proceso->metricas->subidas_memoria_principal += marcos_asignados_en_esta_operacion;
        proceso->metricas->timestamp_ultimo_acceso = time(NULL);
        pthread_mutex_unlock(&proceso->metricas->mutex_metricas);
    }

    // ========== LOG FINAL ==========
    log_info(logger, "PID: %d - Asignación de marcos completada exitosamente - %d marcos asignados", 
             pid, marcos_asignados_en_esta_operacion);
    log_debug(logger, "PID: %d - Estado final: %d/%d páginas asignadas", 
              pid, proceso->estructura_paginas->paginas_asignadas, proceso->estructura_paginas->paginas_totales);
    
    return MEMORIA_OK;
}

// ============================================================================
// FUNCIONES DE INCREMENTO DE MÉTRICAS
// ============================================================================

// ============================================================================
// FUNCIONES DE COMUNICACIÓN Y DELEGACIÓN
// ============================================================================

/**
 * @brief Genera un timestamp en formato YYYYMMDD_HHMMSS para nombres de archivo
 * 
 * @return String con el timestamp (debe ser liberado por el llamador)
 */
static char* generar_timestamp(void) {
    time_t tiempo_actual = time(NULL);
    struct tm* tiempo_local = localtime(&tiempo_actual);
    
    char* timestamp = malloc(20); // YYYYMMDD_HHMMSS + \0
    if (!timestamp) {
        log_error(logger, "Error al asignar memoria para timestamp");
        return NULL;
    }
    
    strftime(timestamp, 20, "%Y%m%d_%H%M%S", tiempo_local);
    return timestamp;
}

/**
 * @brief Lee el contenido completo de un marco físico
 * 
 * @param numero_frame Número del marco a leer
 * @param buffer Buffer donde almacenar el contenido (debe ser de TAM_PAGINA bytes)
 * @return true si se leyó correctamente, false en caso de error
 */
static bool leer_contenido_marco(int numero_frame, void* buffer) {
    if (!sistema_memoria || numero_frame < 0 || !buffer) {
        return false;
    }
    
    // Calcular dirección física del marco
    uint32_t direccion_fisica = numero_frame * cfg->TAM_PAGINA;
    
    // Verificar que está dentro del rango de memoria
    if (direccion_fisica + cfg->TAM_PAGINA > cfg->TAM_MEMORIA) {
        log_error(logger, "Marco %d fuera de rango de memoria", numero_frame);
        return false;
    }
    
    // Copiar contenido del marco al buffer
    memcpy(buffer, (char*)sistema_memoria->memoria_principal + direccion_fisica, cfg->TAM_PAGINA);
    return true;
}

/**
 * @brief Obtiene todos los marcos físicos asignados a un proceso en orden de páginas
 * 
 * @param pid PID del proceso
 * @param marcos_out Array donde almacenar los números de marco (debe tener tamaño suficiente)
 * @param cantidad_marcos_out Puntero donde almacenar la cantidad de marcos encontrados
 * @return true si se obtuvieron correctamente, false en caso de error
 */
static bool obtener_marcos_proceso(int pid, int* marcos_out, int* cantidad_marcos_out) {
    if (!marcos_out || !cantidad_marcos_out) {
        return false;
    }
    
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso || !proceso->estructura_paginas) {
        log_error(logger, "PID: %d - Proceso o estructura de páginas no encontrada", pid);
        return false;
    }
    
    t_estructura_paginas* estructura = proceso->estructura_paginas;
    int marcos_encontrados = 0;
    
    // Recorrer todas las páginas del proceso en orden
    for (int numero_pagina = 0; numero_pagina < estructura->paginas_totales; numero_pagina++) {
        t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, numero_pagina);
        
        if (entrada && entrada->presente) {
            marcos_out[marcos_encontrados] = entrada->numero_frame;
            marcos_encontrados++;
            log_trace(logger, "PID: %d - Página %d -> Marco %d", 
                     pid, numero_pagina, entrada->numero_frame);
        } else {
            // Si la página no está presente, esto es un error para memory dump
            log_error(logger, "PID: %d - Página %d no está presente en memoria", pid, numero_pagina);
            return false;
        }
    }
    
    *cantidad_marcos_out = marcos_encontrados;
    log_debug(logger, "PID: %d - Encontrados %d marcos en memoria", pid, marcos_encontrados);
    return true;
}

t_resultado_memoria procesar_memory_dump(int pid) {
    // ========== LOG OBLIGATORIO ==========
    log_info(logger, "## PID: %d - Memory Dump solicitado", pid);
    
    // ========== VALIDACIONES INICIALES ==========
    if (pid < 0) {
        log_error(logger, "PID: %d - PID inválido para memory dump", pid);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    if (!sistema_memoria) {
        log_error(logger, "PID: %d - Sistema de memoria no inicializado", pid);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    if (!cfg || !cfg->DUMP_PATH) {
        log_error(logger, "PID: %d - Configuración DUMP_PATH no disponible", pid);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // ========== VERIFICAR EXISTENCIA DEL PROCESO ==========
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso) {
        log_error(logger, "PID: %d - Proceso no encontrado para memory dump", pid);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    if (proceso->suspendido) {
        log_error(logger, "PID: %d - No se puede hacer dump de proceso suspendido", pid);
        return MEMORIA_ERROR_PROCESO_SUSPENDIDO;
    }
    
    // ========== GENERACIÓN DEL NOMBRE DEL ARCHIVO ==========
    char* timestamp = generar_timestamp();
    if (!timestamp) {
        log_error(logger, "PID: %d - Error al generar timestamp para dump", pid);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Construir path completo: DUMP_PATH + PID-TIMESTAMP.dmp
    char* nombre_archivo = malloc(512);
    if (!nombre_archivo) {
        log_error(logger, "PID: %d - Error al asignar memoria para nombre de archivo", pid);
        free(timestamp);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    snprintf(nombre_archivo, 512, "%s%d-%s.dmp", cfg->DUMP_PATH, pid, timestamp);
    free(timestamp);
    
    log_debug(logger, "PID: %d - Archivo dump: %s", pid, nombre_archivo);
    
    // ========== OBTENER MARCOS DEL PROCESO ==========
    int* marcos_proceso = malloc(sizeof(int) * proceso->estructura_paginas->paginas_totales);
    if (!marcos_proceso) {
        log_error(logger, "PID: %d - Error al asignar memoria para lista de marcos", pid);
        free(nombre_archivo);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    int cantidad_marcos = 0;
    if (!obtener_marcos_proceso(pid, marcos_proceso, &cantidad_marcos)) {
        log_error(logger, "PID: %d - Error al obtener marcos del proceso", pid);
        free(marcos_proceso);
        free(nombre_archivo);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }
    
    // ========== CREAR Y ESCRIBIR ARCHIVO DUMP ==========
    FILE* archivo_dump = fopen(nombre_archivo, "wb");
    if (!archivo_dump) {
        log_error(logger, "PID: %d - Error al crear archivo dump: %s", pid, nombre_archivo);
        free(marcos_proceso);
        free(nombre_archivo);
        return MEMORIA_ERROR_ARCHIVO;
    }
    
    // Buffer para leer el contenido de cada marco
    void* buffer_pagina = malloc(cfg->TAM_PAGINA);
    if (!buffer_pagina) {
        log_error(logger, "PID: %d - Error al asignar buffer para lectura de páginas", pid);
        fclose(archivo_dump);
        free(marcos_proceso);
        free(nombre_archivo);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Escribir contenido de cada marco al archivo en orden de páginas
    size_t bytes_escritos_total = 0;
    for (int i = 0; i < cantidad_marcos; i++) {
        int numero_marco = marcos_proceso[i];
        
        // Leer contenido del marco
        if (!leer_contenido_marco(numero_marco, buffer_pagina)) {
            log_error(logger, "PID: %d - Error al leer contenido del marco %d", pid, numero_marco);
            fclose(archivo_dump);
            free(buffer_pagina);
            free(marcos_proceso);
            free(nombre_archivo);
            return MEMORIA_ERROR_LECTURA;
        }
        
        // Escribir contenido al archivo
        size_t bytes_escritos = fwrite(buffer_pagina, 1, cfg->TAM_PAGINA, archivo_dump);
        if (bytes_escritos != cfg->TAM_PAGINA) {
            log_error(logger, "PID: %d - Error al escribir página %d al archivo dump", pid, i);
            fclose(archivo_dump);
            free(buffer_pagina);
            free(marcos_proceso);
            free(nombre_archivo);
            return MEMORIA_ERROR_ESCRITURA;
        }
        
        bytes_escritos_total += bytes_escritos;
        log_trace(logger, "PID: %d - Página %d (Marco %d) escrita al dump", pid, i, numero_marco);
    }
    
    // ========== FINALIZACIÓN ==========
    fclose(archivo_dump);
    free(buffer_pagina);
    free(marcos_proceso);
    
    // ========== LOG FINAL OBLIGATORIO ==========
    log_info(logger, "## PID: %d - Memory Dump generado exitosamente", pid);
    log_info(logger, "   - Archivo: %s", nombre_archivo);
    log_info(logger, "   - Tamaño del proceso: %d bytes", proceso->tamanio);
    log_info(logger, "   - Páginas escritas: %d", cantidad_marcos);
    log_info(logger, "   - Bytes totales escritos: %zu", bytes_escritos_total);
    
    free(nombre_archivo);
    return MEMORIA_OK;
}

bool verificar_espacio_disponible(int tamanio) {
    // Calcular páginas necesarias
    int paginas_necesarias = (tamanio + cfg->TAM_PAGINA - 1) / cfg->TAM_PAGINA;
    log_debug(logger, "Verificación de espacio - Tamaño: %d bytes, Páginas necesarias: %d", 
             tamanio, paginas_necesarias);
    
    // Verificar espacio disponible de forma thread-safe
    pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);
    bool hay_espacio = sistema_memoria->admin_marcos->frames_libres >= paginas_necesarias;
    pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
    
    return hay_espacio;
}

void enviar_instruccion_a_cpu(int pid, int pc, int cliente_socket) {
    // Obtener la instrucción
    t_instruccion* instruccion = get_instruction(pid, pc);
    
    if (instruccion != NULL) {
        // Log obligatorio con formato correcto
        char* args_log = string_new();
        if (instruccion->parametros2 && strlen(instruccion->parametros2) > 0) {
            string_append_with_format(&args_log, " %s", instruccion->parametros2);
            if (instruccion->parametros3 && strlen(instruccion->parametros3) > 0) {
                string_append_with_format(&args_log, " %s", instruccion->parametros3);
            }
        }
        log_info(logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s%s", 
                 pid, pc, instruccion->parametros1, args_log);
        free(args_log);

        // Crear y enviar paquete con la instrucción
        t_paquete* paquete = crear_paquete_op(PEDIR_INSTRUCCION_OP);

        // Siempre enviar 3 parámetros
        char* p1 = instruccion->parametros1 ? instruccion->parametros1 : "";
        char* p2 = instruccion->parametros2 ? instruccion->parametros2 : "";
        char* p3 = instruccion->parametros3 ? instruccion->parametros3 : "";
        
        // Agregar en orden fijo
        agregar_a_paquete(paquete, p1, strlen(p1) + 1);
        agregar_a_paquete(paquete, p2, strlen(p2) + 1);
        agregar_a_paquete(paquete, p3, strlen(p3) + 1);

        // Enviar paquete
        enviar_paquete(paquete, cliente_socket);
        eliminar_paquete(paquete);
        
        // Liberar la instrucción obtenida
        liberar_instruccion(instruccion);
    } else {
        log_error(logger, "No se pudo obtener instrucción - PID: %d, PC: %d", pid, pc);
        
        // Enviar respuesta de error
        t_paquete* paquete_error = crear_paquete_op(ERROR);
        enviar_paquete(paquete_error, cliente_socket);
        eliminar_paquete(paquete_error);
    }
} 