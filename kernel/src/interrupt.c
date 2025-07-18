#include "../headers/planificadores.h"

void iniciar_interrupt_handler()
{
    pthread_t hilo_interrupt_handler;
    if (pthread_create(&hilo_interrupt_handler, NULL, interrupt_handler, NULL) != 0)
    {
        LOG_ERROR(kernel_log, "[INTERRUPT] Error al crear hilo para manejar interrupciones");
        terminar_kernel(EXIT_FAILURE);
    }
    pthread_detach(hilo_interrupt_handler);
    LOG_DEBUG(kernel_log, "[INTERRUPT] Hilo de manejo de interrupciones iniciado correctamente");
}

void *interrupt_handler(void *arg)
{
    LOG_DEBUG(kernel_log, VERDE("=== Interrupt handler iniciado ==="));

    while (1)
    {
        SEM_WAIT(sem_interrupciones);

        LOCK_CON_LOG(mutex_cola_interrupciones);
        
        t_interrupcion *intr = queue_pop(cola_interrupciones);
        UNLOCK_CON_LOG(mutex_cola_interrupciones);

        if (!intr)
        {
            LOG_ERROR(kernel_log, VERDE("[INTERRUPT] Cola de interrupción vacía"));
            terminar_kernel(EXIT_FAILURE);
        }

        interrumpir_ejecucion(intr->cpu_a_desalojar);

        free(intr);
    }
}

void interrupt(cpu *cpu_a_desalojar)
{
    t_interrupcion *nueva = malloc(sizeof(t_interrupcion));
    nueva->cpu_a_desalojar = cpu_a_desalojar;

    LOCK_CON_LOG(mutex_cola_interrupciones);

    queue_push(cola_interrupciones, nueva);
    UNLOCK_CON_LOG(mutex_cola_interrupciones);
    LOG_DEBUG(kernel_log, "[INTERRUPT] Interrupción encolada para desalojar CPU %d (fd=%d)", cpu_a_desalojar->id, cpu_a_desalojar->fd);

    SEM_POST(sem_interrupciones);
}