#include "../headers/planificadores.h"
#include <commons/collections/list.h>

/**
 * @brief Inicializa el manejador de interrupciones creando un hilo dedicado.
 *
 * Crea un hilo que ejecuta la función interrupt_handler para manejar las interrupciones
 * encoladas. Si ocurre un error al crear el hilo, se registra en el log.
 */

void iniciar_interrupt_handler()
{
    pthread_t hilo_interrupt_handler;
    if (pthread_create(&hilo_interrupt_handler, NULL, interrupt_handler, NULL) != 0)
    {
        LOG_TRACE(kernel_log, "[INTERRUPT] Error al crear hilo para manejar interrupciones");
        return;
    }
    pthread_detach(hilo_interrupt_handler);
}

/**
 * @brief Función que maneja las interrupciones encoladas.
 *
 * Espera señales en el semáforo de interrupciones, toma la interrupción de la cola,
 * y ejecuta la función de desalojo de CPU correspondiente. Si la cola está vacía,
 * termina la ejecución del hilo. Libera la memoria de la interrupción procesada.
 *
 * @param arg Argumento no utilizado.
 * @return NULL al finalizar la ejecución.
 */
void *interrupt_handler(void *arg)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    LOG_TRACE(kernel_log, VERDE("=== Interrupt handler iniciado ==="));

    while (1)
    {
        SEM_WAIT(sem_interrupciones);

        LOCK_CON_LOG(mutex_cola_interrupciones);

        t_interrupcion *intr = list_remove(cola_interrupciones, 0);
        UNLOCK_CON_LOG(mutex_cola_interrupciones);

        if (!intr)
        {
            LOG_TRACE(kernel_log, VERDE("[INTERRUPT] Cola de interrupción vacía"));
            return NULL;
        }

        interrumpir_ejecucion(intr->cpu_a_desalojar);

        free(intr);
    }

    return NULL;
}

/**
 * @brief Encola una nueva interrupción para desalojar una CPU específica.
 *
 * Crea una nueva estructura de interrupción, la agrega a la cola protegida por mutex,
 * y señala al semáforo para que el manejador procese la interrupción.
 *
 * @param cpu_a_desalojar Puntero a la CPU que debe ser desalojada.
 */
void interrupt(cpu *cpu_a_desalojar)
{
    t_interrupcion *nueva = malloc(sizeof(t_interrupcion));
    nueva->cpu_a_desalojar = cpu_a_desalojar;

    LOCK_CON_LOG(mutex_cola_interrupciones);

    list_add(cola_interrupciones, nueva);
    UNLOCK_CON_LOG(mutex_cola_interrupciones);
    LOG_TRACE(kernel_log, "[INTERRUPT] Interrupción encolada para desalojar CPU %d (fd=%d)", cpu_a_desalojar->id, cpu_a_desalojar->fd);

    SEM_POST(sem_interrupciones);
}