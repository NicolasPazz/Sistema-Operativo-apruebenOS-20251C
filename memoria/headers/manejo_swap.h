#ifndef MANEJO_SWAP_H
#define MANEJO_SWAP_H

#include "estructuras.h"
#include <stdbool.h>

// ============================================================================
// FUNCIONES DE SUSPENSIÓN Y REANUDACIÓN DE PROCESOS
// ============================================================================

/**
 * @brief Suspende un proceso completo escribiendo todas sus páginas a SWAP
 * 
 * Esta función implementa la suspensión de procesos según la consigna del TP:
 * - Solo guarda el CONTENIDO de las páginas (no las tablas de páginas)
 * - Se usa específicamente para el planificador de mediano plazo
 * - Las estructuras administrativas permanecen en memoria
 * 
 * @param pid PID del proceso a suspender
 * @return 1 si la suspensión fue exitosa, 0 en caso de error
 */
int suspender_proceso_completo(int pid);

/**
 * @brief Reanuda un proceso suspendido cargando todas sus páginas desde SWAP
 * 
 * @param pid PID del proceso a reanudar
 * @return 1 si la reanudación fue exitosa, 0 en caso de error
 */
int reanudar_proceso_suspendido(int pid);

// ============================================================================
// FUNCIONES DE GESTIÓN DE PÁGINAS EN SWAP
// ============================================================================

/**
 * @brief Escribe una página específica de un proceso a SWAP
 * 
 * @param pid PID del proceso propietario
 * @param numero_pagina Número de página dentro del proceso
 * @param contenido Contenido de la página a escribir
 * @return 1 si la escritura fue exitosa, 0 en caso de error
 */
int escribir_pagina_proceso_swap(int pid, int numero_pagina, void* contenido);

/**
 * @brief Lee una página específica de un proceso desde SWAP
 * 
 * @param pid PID del proceso propietario
 * @param numero_pagina Número de página dentro del proceso
 * @return Puntero al contenido de la página, NULL en caso de error
 */
void* leer_pagina_proceso_swap(int pid, int numero_pagina);

// ============================================================================
// FUNCIONES DE GESTIÓN DE ESPACIO EN SWAP
// ============================================================================

/**
 * @brief Verifica si hay suficiente espacio en SWAP para todas las páginas de un proceso
 * 
 * @param pid PID del proceso
 * @return 1 si hay espacio suficiente, 0 en caso contrario
 */
int asignar_espacio_swap_proceso(int pid);

/**
 * @brief Libera todo el espacio de SWAP ocupado por un proceso
 * 
 * @param pid PID del proceso
 */
void liberar_espacio_swap_proceso(int pid);

/**
 * @brief Verifica si un proceso tiene páginas almacenadas en SWAP
 * 
 * @param pid PID del proceso
 * @return true si el proceso tiene páginas en SWAP, false en caso contrario
 */
bool proceso_tiene_paginas_en_swap(int pid);

// ============================================================================
// FUNCIONES AUXILIARES PARA GESTIÓN SWAP
// ============================================================================

/**
 * @brief Verifica si un proceso está actualmente suspendido
 * 
 * @param pid PID del proceso a verificar
 * @return true si está suspendido, false en caso contrario
 */
bool proceso_esta_suspendido(int pid);

/**
 * @brief Obtiene la cantidad de páginas que tiene un proceso en SWAP
 * 
 * @param pid PID del proceso
 * @return Cantidad de páginas en SWAP o 0 si no hay
 */
int obtener_paginas_en_swap(int pid);

/**
 * @brief Obtiene estadísticas del sistema SWAP
 * 
 * @param total_entradas_out Puntero donde almacenar total de entradas SWAP
 * @param espacio_usado_out Puntero donde almacenar espacio usado en bytes
 */
void obtener_estadisticas_swap(int* total_entradas_out, size_t* espacio_usado_out);

/**
 * @brief Lista todos los procesos que están actualmente en SWAP
 * 
 * @return Lista de PIDs en SWAP (debe ser liberada por el llamador)
 */
t_list* listar_procesos_en_swap(void);

#endif 