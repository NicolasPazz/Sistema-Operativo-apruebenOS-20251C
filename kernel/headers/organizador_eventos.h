#ifndef ORGANIZADOR_EVENTOS_H
#define ORGANIZADOR_EVENTOS_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

/**
 * @brief Estado del kernel para coordinación de terminación
 */
typedef enum {
    KERNEL_RUNNING,     // Kernel funcionando normalmente
    KERNEL_STOPPING,    // Señal de parada recibida, iniciando terminación
    KERNEL_STOPPED      // Todos los hilos terminados, listo para cleanup
} t_kernel_state;

// Variables globales para coordinación de terminación
extern t_kernel_state kernel_state;
extern pthread_mutex_t mutex_kernel_state;
extern sem_t sem_finalizacion_completa;
extern volatile int hilos_activos;
extern pthread_mutex_t mutex_hilos_activos;

/**
 * @brief Inicializar el organizador de eventos
 */
void inicializar_organizador_eventos(void);

/**
 * @brief Destruir recursos del organizador de eventos
 */
void destruir_organizador_eventos(void);

/**
 * @brief Verificar si el kernel debe continuar ejecutándose
 * @return true si debe continuar, false si debe terminar
 */
bool kernel_debe_continuar(void);

/**
 * @brief Iniciar proceso de terminación ordenada
 */
void iniciar_terminacion_kernel(void);

/**
 * @brief Registrar que un hilo ha iniciado
 */
void registrar_hilo_activo(void);

/**
 * @brief Registrar que un hilo ha terminado
 */
void registrar_hilo_terminado(void);

/**
 * @brief Esperar a que todos los hilos terminen
 */
void esperar_terminacion_todos_hilos(void);

/**
 * @brief Despertar todos los hilos bloqueados en semáforos
 */
void despertar_todos_hilos(void);

/**
 * @brief Obtener estado actual del kernel
 */
t_kernel_state obtener_estado_kernel(void);

/**
 * @brief Invalidar todos los timers de suspensión activos
 */
void invalidar_todos_timers_suspension(void);

/**
 * @brief Cancelar todas las operaciones de memoria pendientes
 */
void cancelar_operaciones_memoria_pendientes(void);

#endif // ORGANIZADOR_EVENTOS_H 