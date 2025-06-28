#ifndef BLOQUEO_PAGINAS_H
#define BLOQUEO_PAGINAS_H

#include "estructuras.h"
#include <stdbool.h>
#include <pthread.h>

// ============================================================================
// FUNCIONES PRINCIPALES DE BLOQUEO DE MARCOS FÍSICOS
// ============================================================================

/**
 * @brief Bloquea un marco físico para operaciones críticas
 * @param numero_frame Número del marco físico a bloquear
 * @param operacion Descripción de la operación que requiere el bloqueo
 * @return true si el bloqueo fue exitoso, false si ya estaba bloqueado o error
 */
bool bloquear_marco(int numero_frame, const char* operacion);

/**
 * @brief Desbloquea un marco físico previamente bloqueado
 * @param numero_frame Número del marco físico a desbloquear
 * @param operacion Descripción de la operación que libera el bloqueo
 * @return true si el desbloqueo fue exitoso, false si error
 */
bool desbloquear_marco(int numero_frame, const char* operacion);

/**
 * @brief Verifica si un marco físico está bloqueado
 * @param numero_frame Número del marco físico a verificar
 * @return true si está bloqueado, false si está libre
 */
bool marco_esta_bloqueado(int numero_frame);

// ============================================================================
// FUNCIONES DE BLOQUEO MASIVO POR PROCESO
// ============================================================================

/**
 * @brief Bloquea todos los marcos físicos de un proceso
 * @param pid PID del proceso cuyos marcos se bloquearán
 * @param operacion Descripción de la operación que requiere el bloqueo
 * @return Cantidad de marcos bloqueados exitosamente, -1 si error
 */
int bloquear_marcos_proceso(int pid, const char* operacion);

/**
 * @brief Desbloquea todos los marcos físicos de un proceso
 * @param pid PID del proceso cuyos marcos se desbloquearán  
 * @param operacion Descripción de la operación que libera el bloqueo
 * @return Cantidad de marcos desbloqueados exitosamente, -1 si error
 */
int desbloquear_marcos_proceso(int pid, const char* operacion);

// ============================================================================
// FUNCIONES DE UTILIDAD Y HELPERS
// ============================================================================

/**
 * @brief Bloquea el marco físico correspondiente a una página específica
 * @param pid PID del proceso
 * @param numero_pagina Número de página lógica
 * @param operacion Descripción de la operación que requiere el bloqueo
 * @return true si el bloqueo fue exitoso, false si error
 */
bool bloquear_marco_por_pagina(int pid, int numero_pagina, const char* operacion);

/**
 * @brief Desbloquea el marco físico correspondiente a una página específica
 * @param pid PID del proceso
 * @param numero_pagina Número de página lógica
 * @param operacion Descripción de la operación que libera el bloqueo
 * @return true si el desbloqueo fue exitoso, false si error
 */
bool desbloquear_marco_por_pagina(int pid, int numero_pagina, const char* operacion);

/**
 * @brief Verifica si el marco de una página específica está bloqueado
 * @param pid PID del proceso
 * @param numero_pagina Número de página lógica
 * @return true si está bloqueado, false si está libre
 */
bool marco_pagina_esta_bloqueado(int pid, int numero_pagina);

/**
 * @brief Obtiene el número de página de un proceso que está mapeada a un marco específico
 * @param pid PID del proceso
 * @param numero_marco Número del marco físico
 * @return Número de página mapeada al marco, -1 si no se encuentra
 */
int obtener_numero_pagina_de_marco(int pid, int numero_marco);

/**
 * @brief Obtiene el thread que tiene bloqueado un marco
 * @param numero_frame Número del marco físico
 * @return Thread ID del bloqueador, 0 si no está bloqueado
 */
pthread_t obtener_thread_bloqueador_marco(int numero_frame);

/**
 * @brief Obtiene la operación actual que tiene bloqueado un marco
 * @param numero_frame Número del marco físico
 * @param operacion_out Buffer donde se escribirá la operación (mín 64 bytes)
 * @return true si se obtuvo la operación, false si no está bloqueado o error
 */
bool obtener_operacion_marco(int numero_frame, char* operacion_out);

/**
 * @brief Obtiene estadísticas globales de bloqueos
 * @param marcos_bloqueados_out Cantidad de marcos actualmente bloqueados
 * @param procesos_con_bloqueos_out Cantidad de procesos con al menos un marco bloqueado
 */
void obtener_estadisticas_bloqueos_marcos(int* marcos_bloqueados_out, int* procesos_con_bloqueos_out);

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN Y LIMPIEZA
// ============================================================================

/**
 * @brief Inicializa el sistema de bloqueo para un marco específico (lazy)
 * @param numero_frame Número del marco a inicializar
 * @return true si la inicialización fue exitosa, false si error
 */
bool inicializar_bloqueo_marco(int numero_frame);

/**
 * @brief Destruye el sistema de bloqueo de un marco específico
 * @param numero_frame Número del marco a limpiar
 */
void destruir_bloqueo_marco(int numero_frame);

#endif // BLOQUEO_PAGINAS_H 