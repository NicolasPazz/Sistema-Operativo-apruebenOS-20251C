#ifndef MANEJO_MEMORIA_H
#define MANEJO_MEMORIA_H

#include <stdint.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <commons/temporal.h>
#include <pthread.h>

#include "estructuras.h"

// ============================================================================
// FUNCIONES DEL ADMINISTRADOR CENTRALIZADO DE MARCOS FÍSICOS
// ============================================================================

/**
 * @brief Inicializa el administrador de marcos físicos
 * 
 * Crea y configura el administrador centralizado que gestiona todos los
 * marcos de memoria física del sistema.
 * 
 * @param cantidad_frames Cantidad total de frames (TAM_MEMORIA / TAM_PAGINA)
 * @param tam_pagina Tamaño de cada página en bytes
 * @return Puntero al administrador creado o NULL si hay error
 */
t_administrador_marcos* crear_administrador_marcos(int cantidad_frames, int tam_pagina);

/**
 * @brief Destruye el administrador de marcos y libera recursos
 * 
 * @param admin Puntero al administrador a destruir
 */
void destruir_administrador_marcos(t_administrador_marcos* admin);

/**
 * @brief Asigna un marco libre del pool centralizado
 * 
 * Busca y asigna un marco libre de manera eficiente (O(1)) usando
 * la lista de marcos libres.
 * 
 * @param pid PID del proceso solicitante
 * @param numero_pagina Número de página lógica del proceso
 * @return Número de marco asignado o -1 si no hay marcos disponibles
 */
int asignar_marco_libre(int pid, int numero_pagina);

/**
 * @brief Libera un marco específico y lo devuelve al pool
 * 
 * @param numero_frame Número del marco a liberar
 * @return MEMORIA_OK si se liberó correctamente, error en caso contrario
 */
t_resultado_memoria liberar_marco(int numero_frame);

/**
 * @brief Obtiene información de un marco específico
 * 
 * @param numero_frame Número del marco
 * @return Puntero al frame o NULL si no existe
 */
t_frame* obtener_frame(int numero_frame);

/**
 * @brief Obtiene la cantidad de marcos libres disponibles
 * 
 * @return Cantidad de marcos libres
 */
int obtener_marcos_libres(void);

/**
 * @brief Obtiene estadísticas del administrador de marcos
 * 
 * @param total_frames Puntero donde almacenar total de frames
 * @param frames_libres Puntero donde almacenar frames libres
 * @param frames_ocupados Puntero donde almacenar frames ocupados
 */
void obtener_estadisticas_marcos(int* total_frames, int* frames_libres, int* frames_ocupados);

// ============================================================================
// FUNCIONES DE GESTIÓN DE PROCESOS EN MEMORIA
// ============================================================================

/**
 * @brief Crea un nuevo proceso en el sistema de memoria
 * 
 * Inicializa todas las estructuras necesarias: proceso, jerarquía de páginas,
 * métricas e instrucciones. NO asigna marcos físicos aún.
 * 
 * @param pid PID del proceso
 * @param tamanio Tamaño del proceso en bytes
 * @param nombre_archivo Nombre del archivo de pseudocódigo
 * @return MEMORIA_OK si se creó correctamente, error en caso contrario
 */
t_resultado_memoria crear_proceso_en_memoria(int pid, int tamanio, char* nombre_archivo);

/**
 * @brief Asigna marcos físicos a todas las páginas de un proceso
 * 
 * Asigna marcos del pool centralizado a todas las páginas necesarias
 * del proceso y actualiza las tablas de páginas correspondientes.
 * 
 * @param pid PID del proceso
 * @return MEMORIA_OK si se asignaron todos los marcos, error en caso contrario
 */
t_resultado_memoria asignar_marcos_proceso(int pid);

/**
 * @brief Finaliza un proceso y libera todos sus recursos
 * 
 * Libera marcos físicos, destruye jerarquía de páginas, imprime métricas
 * y elimina el proceso del sistema.
 * 
 * @param pid PID del proceso a finalizar
 * @return MEMORIA_OK si se finalizó correctamente, error en caso contrario
 */
t_resultado_memoria finalizar_proceso_en_memoria(int pid);

/**
 * @brief Verifica si un proceso existe y está activo
 * 
 * @param pid PID del proceso
 * @return true si existe y está activo, false en caso contrario
 */
bool proceso_existe(int pid);

/**
 * @brief Obtiene información completa de un proceso
 * 
 * @param pid PID del proceso
 * @return Puntero al proceso o NULL si no existe
 */
t_proceso_memoria* obtener_proceso(int pid);

// ============================================================================
// FUNCIONES DE PAGINACIÓN JERÁRQUICA MULTINIVEL
// ============================================================================

/**
 * @brief Crea la estructura de paginación jerárquica para un proceso
 * 
 * Inicializa la jerarquía multinivel completa con la tabla raíz y
 * todas las estructuras necesarias según la configuración.
 * 
 * @param pid PID del proceso propietario
 * @param tamanio_proceso Tamaño del proceso en bytes
 * @return Puntero a la estructura creada o NULL si hay error
 */
t_estructura_paginas* crear_estructura_paginas(int pid, int tamanio_proceso);

/**
 * @brief Destruye la estructura de paginación de un proceso
 * 
 * Libera recursivamente todas las tablas de la jerarquía.
 * 
 * @param estructura Puntero a la estructura a destruir
 */
void destruir_estructura_paginas(t_estructura_paginas* estructura);

/**
 * @brief Busca la entrada de tabla correspondiente a una página lógica
 * 
 * Navega por la jerarquía multinivel para encontrar la entrada que
 * corresponde al número de página especificado.
 * 
 * @param estructura Estructura de paginación del proceso
 * @param numero_pagina Número de página lógica (0, 1, 2, ...)
 * @return Puntero a la entrada encontrada o NULL si no existe
 */
t_entrada_tabla* buscar_entrada_tabla(t_estructura_paginas* estructura, int numero_pagina);

/**
 * @brief Crea las tablas intermedias necesarias para una página
 * 
 * Si no existen las tablas intermedias en la jerarquía para llegar
 * a una página específica, las crea automáticamente.
 * 
 * @param estructura Estructura de paginación del proceso
 * @param numero_pagina Número de página lógica
 * @return Puntero a la entrada de tabla creada o NULL si hay error
 */
t_entrada_tabla* crear_entrada_tabla_si_no_existe(t_estructura_paginas* estructura, int numero_pagina);

/**
 * @brief Calcula los índices de tabla para una página en la jerarquía
 * 
 * Descompone el número de página en los índices correspondientes
 * para cada nivel de la jerarquía multinivel.
 * 
 * @param numero_pagina Número de página lógica
 * @param entradas_por_tabla Entradas por tabla (configuración)
 * @param cantidad_niveles Cantidad de niveles (configuración)
 * @param indices Array donde almacenar los índices calculados
 */
void calcular_indices_jerarquia(int numero_pagina, int entradas_por_tabla, 
                                int cantidad_niveles, int* indices);

// ============================================================================
// FUNCIONES DE ACCESO A MEMORIA FÍSICA
// ============================================================================

/**
 * @brief Lee datos de una dirección física específica
 * 
 * @param direccion_fisica Dirección física en memoria
 * @param tamanio Cantidad de bytes a leer
 * @param buffer Buffer donde almacenar los datos leídos
 * @return MEMORIA_OK si se leyó correctamente, error en caso contrario
 */
t_resultado_memoria leer_memoria_fisica(uint32_t direccion_fisica, int tamanio, void* buffer);

/**
 * @brief Escribe datos en una dirección física específica
 * 
 * @param direccion_fisica Dirección física en memoria
 * @param datos Datos a escribir
 * @param tamanio Cantidad de bytes a escribir
 * @return MEMORIA_OK si se escribió correctamente, error en caso contrario
 */
t_resultado_memoria escribir_memoria_fisica(uint32_t direccion_fisica, void* datos, int tamanio);

/**
 * @brief Lee una página completa de un marco específico
 * 
 * @param numero_frame Número del marco a leer
 * @param buffer Buffer donde almacenar el contenido (debe ser de TAM_PAGINA bytes)
 * @return MEMORIA_OK si se leyó correctamente, error en caso contrario
 */
t_resultado_memoria leer_pagina_memoria(int numero_frame, void* buffer);

/**
 * @brief Escribe una página completa en un marco específico
 * 
 * @param numero_frame Número del marco donde escribir
 * @param contenido Contenido a escribir (debe ser de TAM_PAGINA bytes)
 * @return MEMORIA_OK si se escribió correctamente, error en caso contrario
 */
t_resultado_memoria escribir_pagina_memoria(int numero_frame, void* contenido);

// ============================================================================
// FUNCIONES DE SUSPENSIÓN Y REANUDACIÓN (SWAP)
// ============================================================================

/**
 * @brief Suspende un proceso llevando sus páginas a SWAP (wrapper de alto nivel)
 * 
 * Esta función proporciona una interfaz de alto nivel para la suspensión,
 * incluyendo validaciones y manejo del resultado según el tipo t_resultado_memoria.
 * 
 * @param pid PID del proceso a suspender
 * @return MEMORIA_OK si se suspendió correctamente, error en caso contrario
 */
t_resultado_memoria suspender_proceso_en_memoria(int pid);

/**
 * @brief Reanuda un proceso cargando sus páginas desde SWAP (wrapper de alto nivel)
 * 
 * Esta función proporciona una interfaz de alto nivel para la reanudación,
 * incluyendo validaciones y manejo del resultado según el tipo t_resultado_memoria.
 * 
 * @param pid PID del proceso a reanudar
 * @return MEMORIA_OK si se reanudó correctamente, error en caso contrario
 */
t_resultado_memoria reanudar_proceso_en_memoria(int pid);

/**
 * @brief Suspende un proceso completo escribiendo todas sus páginas a SWAP
 * 
 * Mueve todas las páginas del proceso desde memoria física al archivo
 * de SWAP y libera los marcos correspondientes.
 * 
 * @param pid PID del proceso a suspender
 * @return 1 si se suspendió correctamente, 0 en caso de error
 */
int suspender_proceso_completo(int pid);

/**
 * @brief Reanuda un proceso suspendido cargando sus páginas desde SWAP
 * 
 * Lee todas las páginas del proceso desde SWAP, asigna nuevos marcos
 * y actualiza las tablas de páginas.
 * 
 * @param pid PID del proceso a reanudar
 * @return 1 si se reanudó correctamente, 0 en caso de error
 */
int reanudar_proceso_suspendido(int pid);

/**
 * @brief Verifica si un proceso está suspendido
 * 
 * @param pid PID del proceso a verificar
 * @return true si el proceso está suspendido, false en caso contrario
 */
bool proceso_esta_suspendido(int pid);

// ============================================================================
// FUNCIONES DE ACCESO A MEMORIA FÍSICA - AUXILIARES
// ============================================================================

/**
 * @brief Destruye un proceso y libera todos sus recursos
 * 
 * @param proceso Puntero al proceso a destruir
 */
void destruir_proceso(t_proceso_memoria* proceso);

/**
 * @brief Destruye las métricas de un proceso
 * 
 * @param metricas Puntero a las métricas a destruir
 */
void destruir_metricas_proceso(t_metricas_proceso* metricas);

/**
 * @brief Destruye recursivamente una tabla de páginas
 * 
 * @param tabla Puntero a la tabla a destruir
 */
void destruir_tabla_paginas_recursiva(t_tabla_paginas* tabla);

// ============================================================================
// FUNCIONES DE MÉTRICAS - Las 6 métricas EXACTAS de la consigna
// ============================================================================

/**
 * @brief Incrementa el contador de accesos a tablas de páginas
 * @param pid PID del proceso
 */
void incrementar_accesos_tabla_paginas(int pid);

/**
 * @brief Incrementa el contador de instrucciones solicitadas
 * @param pid PID del proceso
 */
void incrementar_instrucciones_solicitadas(int pid);

/**
 * @brief Incrementa el contador de bajadas a SWAP
 * @param pid PID del proceso
 */
void incrementar_bajadas_swap(int pid);

/**
 * @brief Incrementa el contador de subidas a Memoria Principal
 * @param pid PID del proceso
 */
void incrementar_subidas_memoria_principal(int pid);

/**
 * @brief Incrementa el contador de lecturas de memoria
 * @param pid PID del proceso
 */
void incrementar_lecturas_memoria(int pid);

/**
 * @brief Incrementa el contador de escrituras de memoria
 * @param pid PID del proceso
 */
void incrementar_escrituras_memoria(int pid);

/**
 * @brief Obtiene las métricas de un proceso
 * @param pid PID del proceso
 * @return Puntero a las métricas o NULL si no existe
 */
t_metricas_proceso* obtener_metricas_proceso(int pid);

/**
 * @brief Imprime las métricas de un proceso según formato de consigna
 * 
 * Formato obligatorio: "Acc.T.Pag: <ATP>; Inst.Sol.: <Inst.Sol.>; SWAP: <SWAP>; 
 * Mem.Prin.: <Mem.Prin.>; Lec.Mem.: <Lec.Mem.>; Esc.Mem.: <Esc.Mem.>"
 * 
 * @param pid PID del proceso
 */
void imprimir_metricas_proceso(int pid);

// ============================================================================
// FUNCIONES DE INSTRUCCIONES
// ============================================================================

/**
 * @brief Carga las instrucciones de un proceso desde archivo
 * 
 * Según la consigna: "Por cada PID del sistema, se deberá leer su archivo 
 * de pseudocódigo y guardar de forma estructurada las instrucciones del mismo 
 * para poder devolverlas una a una a pedido de la CPU"
 * 
 * @param pid PID del proceso
 * @param nombre_archivo Nombre del archivo de pseudocódigo
 * @return MEMORIA_OK si se cargaron correctamente, error en caso contrario
 */
t_resultado_memoria cargar_instrucciones_proceso(int pid, char* nombre_archivo);

/**
 * @brief Obtiene una instrucción específica de un proceso
 * 
 * @param pid PID del proceso
 * @param pc Program Counter (número de instrucción)
 * @return Puntero a la instrucción o NULL si no existe
 */
t_instruccion* obtener_instruccion_proceso(int pid, int pc);

/**
 * @brief Libera las instrucciones de un proceso
 * 
 * @param instrucciones Lista de instrucciones a liberar
 */
void liberar_instrucciones_proceso(t_list* instrucciones);

/**
 * @brief Libera una instrucción individual
 * 
 * @param instruccion Puntero a la instrucción a liberar
 */
void liberar_instruccion(t_instruccion* instruccion);

// ============================================================================
// FUNCIONES DE DUMP DE MEMORIA
// ============================================================================

/**
 * @brief Genera un dump completo de la memoria de un proceso
 * 
 * Crea un archivo "<PID>-<TIMESTAMP>.dmp" con todo el contenido
 * de memoria del proceso según la consigna.
 * 
 * @param pid PID del proceso
 * @return MEMORIA_OK si se generó correctamente, error en caso contrario
 */
t_resultado_memoria generar_dump_proceso(int pid);

// ============================================================================
// FUNCIONES DE UTILIDAD Y VALIDACIÓN
// ============================================================================

/**
 * @brief Valida una dirección lógica para un proceso
 * 
 * @param pid PID del proceso
 * @param direccion_logica Dirección lógica a validar
 * @return true si es válida, false en caso contrario
 */
bool validar_direccion_logica(int pid, uint32_t direccion_logica);

/**
 * @brief Calcula el número de página de una dirección lógica
 * 
 * @param direccion_logica Dirección lógica
 * @param tam_pagina Tamaño de página
 * @return Número de página
 */
int calcular_numero_pagina(uint32_t direccion_logica, int tam_pagina);

/**
 * @brief Calcula el desplazamiento dentro de una página
 * 
 * @param direccion_logica Dirección lógica
 * @param tam_pagina Tamaño de página
 * @return Desplazamiento dentro de la página
 */
int calcular_desplazamiento(uint32_t direccion_logica, int tam_pagina);

/**
 * @brief Aplica el retardo configurado de acceso a memoria
 */
void aplicar_retardo_memoria(void);

/**
 * @brief Aplica el retardo configurado de acceso a SWAP
 */
void aplicar_retardo_swap(void);

// ============================================================================
// FUNCIONES LEGACY (COMPATIBILIDAD CON CHECKPOINT 2)
// ============================================================================

/**
 * @brief Función legacy para compatibilidad con checkpoint 2
 * @deprecated Usar leer_memoria_fisica en su lugar
 */
void* leer_pagina(int dir_fisica);

/**
 * @brief Función legacy para asignación de frames
 * @deprecated Usar asignar_marco_libre en su lugar
 */
int asignar_frame_libre(int pid, int numero_pagina);

/**
 * @brief Función legacy para liberación de frames
 * @deprecated Usar liberar_marco en su lugar
 */
void liberar_frame(int numero_frame);

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN Y FINALIZACIÓN
// ============================================================================

/**
 * @brief Inicializa el sistema completo de memoria
 * 
 * Crea y configura todos los componentes: administrador de marcos,
 * administrador de SWAP, diccionarios y estructuras auxiliares.
 * 
 * @return MEMORIA_OK si se inicializó correctamente, error en caso contrario
 */
t_resultado_memoria inicializar_sistema_memoria_completo(void);

/**
 * @brief Finaliza el sistema de memoria y libera todos los recursos
 */
void finalizar_sistema_memoria(void);

// ============================================================================
// FUNCIONES DE ACCESO A MEMORIA SEGÚN CONSIGNA
// ============================================================================

// 1. Acceso a tabla de páginas - devuelve número de marco
int acceso_tabla_paginas(int pid, int numero_pagina);

// 2. Acceso a espacio de usuario - lectura/escritura
void* acceso_espacio_usuario_lectura(int pid, int direccion_fisica, int tamanio);
bool acceso_espacio_usuario_escritura(int pid, int direccion_fisica, int tamanio, void* datos);

// 3. Leer página completa desde dirección física - IMPLEMENTADA COMO leer_pagina_marco
// void* leer_pagina_completa(int pid, int direccion_fisica); // ELIMINADA - usar leer_pagina_marco

// 4. Actualizar página completa en dirección física
bool actualizar_pagina_completa(int pid, int direccion_fisica, void* contenido_pagina);

// ============================================================================
// FUNCIONES DE NAVEGACIÓN Y UTILIDADES
// ============================================================================

/**
 * @brief Libera la información de un proceso
 *
 * @param proceso Puntero al proceso a liberar
 */
void liberar_proceso_memoria(t_proceso_memoria* proceso);

// ============================================================================
// FUNCIONES DE COMUNICACIÓN Y DELEGACIÓN
// ============================================================================

/**
 * @brief Procesa una solicitud de memory dump
 * 
 * @param pid PID del proceso
 * @return MEMORIA_OK si se procesó correctamente, error en caso contrario
 */
t_resultado_memoria procesar_memory_dump(int pid);

/**
 * @brief Verifica si hay espacio disponible para un tamaño dado
 * 
 * @param tamanio Tamaño en bytes requerido
 * @return true si hay espacio suficiente, false en caso contrario
 */
bool verificar_espacio_disponible(int tamanio);

/**
 * @brief Obtiene una instrucción y la envía formateada a la CPU
 * 
 * @param pid PID del proceso
 * @param pc Program Counter
 * @param cliente_socket Socket de comunicación con la CPU
 */
void enviar_instruccion_a_cpu(int pid, int pc, int cliente_socket);

#endif
