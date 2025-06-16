#ifndef ESTRUCTURAS_H
#define ESTRUCTURAS_H

#include <stdint.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <commons/bitarray.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include "../../utils/headers/sockets.h"

// ============================================================================
// ESTRUCTURAS PARA ADMINISTRACIÓN CENTRALIZADA DE MARCOS FÍSICOS
// ============================================================================

/**
 * @brief Frame de memoria física - Unidad básica de almacenamiento
 * 
 * Representa un marco físico en la memoria principal. Cada frame puede
 * almacenar exactamente una página de datos de cualquier proceso.
 */
typedef struct {
    int numero_frame;       // Identificador único del frame (0 a N-1)
    bool ocupado;          // true = ocupado, false = libre
    int pid_propietario;   // PID del proceso que ocupa el frame (-1 si libre)
    int numero_pagina;     // Número de página lógica del proceso que contiene
    void* contenido;       // Puntero directo al contenido en memoria_principal
    time_t timestamp_asignacion; // Para algoritmos de reemplazo (futuro)
} t_frame;

/**
 * @brief Administrador centralizado de marcos físicos
 * 
 * Gestiona todos los marcos de memoria física del sistema. Proporciona
 * asignación/liberación eficiente y mantiene estadísticas globales.
 */
typedef struct {
    t_frame* frames;                    // Array de todos los frames del sistema
    int cantidad_total_frames;          // Cantidad total de frames (TAM_MEMORIA / TAM_PAGINA)
    int frames_libres;                  // Contador de frames disponibles
    int frames_ocupados;                // Contador de frames en uso
    
    // Estructuras para búsqueda eficiente
    t_bitarray* bitmap_frames;          // Bitmap: 1=ocupado, 0=libre
    t_list* lista_frames_libres;        // Lista de números de frames libres (para acceso O(1))
    
    // Estadísticas y métricas
    int total_asignaciones;             // Total de asignaciones realizadas
    int total_liberaciones;             // Total de liberaciones realizadas
    
    // Sincronización
    pthread_mutex_t mutex_frames;       // Mutex para operaciones thread-safe
} t_administrador_marcos;

// ============================================================================
// ESTRUCTURAS PARA PAGINACIÓN JERÁRQUICA MULTINIVEL POR PROCESO
// ============================================================================

/**
 * @brief Entrada de tabla de páginas
 * 
 * Cada entrada puede apuntar a:
 * - Otra tabla de páginas (niveles intermedios)
 * - Un marco físico (nivel hoja)
 */
typedef struct {
    bool presente;              // 1 = página en memoria, 0 = no asignada o en SWAP
    bool modificado;            // Dirty bit - 1 = página modificada
    bool referenciado;          // Reference bit - para algoritmos de reemplazo
    uint32_t numero_frame;      // Número de marco físico (si presente=true)
    
    // Para niveles intermedios (no hoja)
    void* tabla_siguiente;      // Puntero a tabla del siguiente nivel (NULL si es hoja)
    
    // Metadatos adicionales
    time_t timestamp_acceso;    // Último acceso (para algoritmos LRU)
} t_entrada_tabla;

/**
 * @brief Tabla de páginas de un nivel específico
 * 
 * Contiene un array de entradas. El tamaño del array es siempre
 * ENTRADAS_POR_TABLA (configurable).
 */
typedef struct {
    t_entrada_tabla* entradas;      // Array de entradas (tamaño = ENTRADAS_POR_TABLA)
    int nivel;                      // Nivel en la jerarquía (0=hoja, mayor=más alto)
    int entradas_utilizadas;        // Cantidad de entradas con contenido válido
    bool inicializada;              // true = tabla creada e inicializada
    
    // Metadatos para optimización
    int pid_propietario;            // PID del proceso propietario
    pthread_mutex_t mutex_tabla;    // Mutex para acceso concurrente a esta tabla
} t_tabla_paginas;

/**
 * @brief Estructura de paginación completa de un proceso
 * 
 * Mantiene la jerarquía multinivel completa de un proceso específico.
 * Cada proceso tiene su propia instancia de esta estructura.
 */
typedef struct {
    int pid;                        // PID del proceso propietario
    
    // Configuración de la jerarquía (copiada de cfg para acceso rápido)
    int cantidad_niveles;           // Niveles de paginación (ej: 3)
    int entradas_por_tabla;         // Entradas por tabla (ej: 4)
    int tam_pagina;                 // Tamaño de página en bytes (ej: 64)
    
    // Información del proceso
    int tamanio_proceso;            // Tamaño total del proceso en bytes
    int paginas_totales;            // Cantidad total de páginas necesarias
    int paginas_asignadas;          // Páginas que tienen marco asignado
    int paginas_en_swap;            // Páginas que están en SWAP
    
    // Jerarquía de tablas
    t_tabla_paginas* tabla_raiz;    // Tabla del nivel más alto (punto de entrada)
    
    // Estado del proceso
    bool activo;                    // true = proceso activo
    bool suspendido;                // true = proceso suspendido (todas las páginas en SWAP)
    
    // Sincronización
    pthread_mutex_t mutex_estructura; // Mutex para operaciones en esta estructura
} t_estructura_paginas;

// ============================================================================
// ESTRUCTURAS PARA MANEJO DE SWAP
// ============================================================================

/**
 * @brief Entrada de SWAP - Mapea páginas a posiciones en swapfile.bin
 */
typedef struct {
    bool ocupado;               // true = posición ocupada, false = libre
    int pid_propietario;        // PID del proceso propietario (-1 si libre)
    int numero_pagina;          // Número de página lógica del proceso
    off_t offset_archivo;       // Offset en bytes dentro del swapfile.bin
    time_t timestamp_escritura; // Cuándo se escribió a SWAP
} t_entrada_swap;

/**
 * @brief Administrador de SWAP
 * 
 * Gestiona el archivo swapfile.bin y mantiene el mapeo de páginas
 * suspendidas a posiciones en el archivo.
 */
typedef struct {
    char* path_archivo;             // Ruta completa del swapfile.bin
    int fd_swap;                    // File descriptor del archivo
    
    // Configuración
    int tamanio_swap;               // Tamaño total del swap en bytes
    int cantidad_paginas_swap;      // Cantidad máxima de páginas en swap
    int tam_pagina;                 // Tamaño de página (copiado de cfg)
    
    // Gestión de espacio
    t_entrada_swap* entradas;       // Array de entradas de swap
    int paginas_libres_swap;        // Páginas disponibles en swap
    int paginas_ocupadas_swap;      // Páginas en uso en swap
    
    // Estructuras para búsqueda eficiente
    t_list* posiciones_libres;      // Lista de posiciones libres (acceso O(1))
    
    // Estadísticas
    int total_escrituras_swap;      // Total de páginas escritas a swap
    int total_lecturas_swap;        // Total de páginas leídas desde swap
    
    // Sincronización
    pthread_mutex_t mutex_swap;     // Mutex para operaciones thread-safe
} t_administrador_swap;

// ============================================================================
// ESTRUCTURAS PARA MÉTRICAS Y PROCESOS
// ============================================================================

/**
 * @brief Métricas detalladas por proceso
 * 
 * Las 6 métricas EXACTAS especificadas en la consigna del TP.
 */
typedef struct {
    int pid;                        // PID del proceso
    
    // Las 6 métricas obligatorias de la consigna:
    int accesos_tabla_paginas;      // Cantidad de accesos a Tablas de Páginas
    int instrucciones_solicitadas;  // Cantidad de Instrucciones solicitadas
    int bajadas_swap;               // Cantidad de bajadas a SWAP
    int subidas_memoria_principal;  // Cantidad de subidas a Memoria Principal
    int lecturas_memoria;           // Cantidad de Lecturas de memoria
    int escrituras_memoria;         // Cantidad de Escrituras de memoria
    
    // Métricas adicionales para análisis
    time_t timestamp_creacion;      // Cuándo se creó el proceso
    time_t timestamp_ultimo_acceso; // Último acceso a memoria
    
    // Sincronización
    pthread_mutex_t mutex_metricas; // Mutex para actualización thread-safe
} t_metricas_proceso;

/**
 * @brief Información completa de un proceso en memoria
 * 
 * Estructura principal que agrupa toda la información de un proceso
 * en el sistema de memoria.
 */
typedef struct {
    int pid;                        // PID del proceso
    int tamanio;                    // Tamaño del proceso en bytes
    
    // Estado del proceso
    bool activo;                    // true = proceso activo en el sistema
    bool suspendido;                // true = proceso suspendido (en SWAP)
    
    // Estructuras asociadas
    t_estructura_paginas* estructura_paginas; // Jerarquía de paginación
    t_metricas_proceso* metricas;   // Métricas del proceso
    t_list* instrucciones;          // Lista de instrucciones cargadas
    
    // Metadatos
    char* nombre_archivo;           // Nombre del archivo de pseudocódigo
    time_t timestamp_creacion;      // Cuándo se creó el proceso
    time_t timestamp_ultimo_uso;    // Último uso del proceso
} t_proceso_memoria;

// ============================================================================
// ESTRUCTURA PRINCIPAL DEL SISTEMA DE MEMORIA
// ============================================================================

/**
 * @brief Sistema completo de memoria
 * 
 * Estructura principal que contiene todos los componentes del sistema
 * de memoria: administradores, diccionarios, configuración y estadísticas.
 */
typedef struct {
    // ========== MEMORIA FÍSICA (ESPACIO DE USUARIO) ==========
    void* memoria_principal;                    // Espacio contiguo de memoria física
    t_administrador_marcos* admin_marcos;       // Administrador centralizado de marcos
    
    // ========== SWAP ==========
    t_administrador_swap* admin_swap;           // Administrador de SWAP
    
    // ========== CONFIGURACIÓN DEL SISTEMA ==========
    int tam_memoria;                            // Tamaño total de memoria física
    int tam_pagina;                             // Tamaño de página
    int entradas_por_tabla;                     // Entradas por tabla de páginas
    int cantidad_niveles;                       // Niveles de paginación
    int retardo_memoria;                        // Retardo de acceso a memoria (ms)
    int retardo_swap;                           // Retardo de acceso a SWAP (ms)
    
    // ========== DICCIONARIOS (ESPACIO DE KERNEL) ==========
    t_dictionary* procesos;                     // PID -> t_proceso_memoria*
    t_dictionary* estructuras_paginas;          // PID -> t_estructura_paginas*
    t_dictionary* metricas_procesos;            // PID -> t_metricas_proceso*
    t_dictionary* process_instructions;         // PID -> t_process_instructions*
    
    // ========== ESTADÍSTICAS GLOBALES ==========
    int procesos_activos;                       // Cantidad de procesos activos
    int procesos_suspendidos;                   // Cantidad de procesos suspendidos
    int memoria_utilizada;                      // Memoria física utilizada (bytes)
    int swap_utilizado;                         // Espacio de swap utilizado (bytes)
    
    // Estadísticas de operaciones
    int total_asignaciones_memoria;             // Total de asignaciones de memoria
    int total_liberaciones_memoria;             // Total de liberaciones de memoria
    int total_suspensiones;                     // Total de suspensiones de procesos
    int total_reanudaciones;                    // Total de reanudaciones de procesos
    
    // ========== SINCRONIZACIÓN ==========
    pthread_mutex_t mutex_sistema;              // Mutex general del sistema
    pthread_mutex_t mutex_procesos;             // Mutex para operaciones con procesos
    pthread_mutex_t mutex_estadisticas;         // Mutex para actualización de estadísticas
} t_sistema_memoria;

// ============================================================================
// ESTRUCTURAS AUXILIARES
// ============================================================================

/**
 * @brief Estructura para almacenar instrucciones de un proceso
 */
typedef struct {
    int pid;                        // PID del proceso
    t_list* instructions;           // Lista de t_extended_instruccion*
    int cantidad_instrucciones;     // Cantidad total de instrucciones
    char* nombre_archivo;           // Nombre del archivo de origen
} t_process_instructions;

/**
 * @brief Resultado de operaciones de memoria
 */
typedef enum {
    MEMORIA_OK = 0,                 // Operación exitosa
    MEMORIA_ERROR_NO_ESPACIO,       // No hay espacio disponible
    MEMORIA_ERROR_PROCESO_NO_EXISTE, // Proceso no existe
    MEMORIA_ERROR_PROCESO_EXISTENTE, // Proceso ya existe
    MEMORIA_ERROR_PROCESO_NO_ENCONTRADO, // Proceso no encontrado
    MEMORIA_ERROR_PAGINA_NO_PRESENTE, // Página no está en memoria
    MEMORIA_ERROR_DIRECCION_INVALIDA, // Dirección fuera de rango
    MEMORIA_ERROR_SWAP_LLENO,       // SWAP lleno
    MEMORIA_ERROR_IO,               // Error de entrada/salida
    MEMORIA_ERROR_MEMORIA_INSUFICIENTE // Error de malloc/calloc
} t_resultado_memoria;

#endif
