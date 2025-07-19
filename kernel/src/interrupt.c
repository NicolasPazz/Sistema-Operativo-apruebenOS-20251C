#include "../headers/planificadores.h"

void iniciar_interrupt_handler()
{
    pthread_t *hilo_interrupt_handler = malloc(sizeof(pthread_t));
    if (pthread_create(hilo_interrupt_handler, NULL, interrupt_handler, NULL) != 0)
    {
        LOG_ERROR(kernel_log, "[INTERRUPT] Error al crear hilo para manejar interrupciones");
        free(hilo_interrupt_handler);
        terminar_kernel(EXIT_FAILURE);
    }
    LOCK_CON_LOG(mutex_hilos);
    list_add(lista_hilos, hilo_interrupt_handler);
    LOG_DEBUG(kernel_log, "Hilo %d agregado", list_size(lista_hilos));
    UNLOCK_CON_LOG(mutex_hilos);
    LOG_DEBUG(kernel_log, "[INTERRUPT] Hilo de manejo de interrupciones iniciado correctamente");
}

void *interrupt_handler(void *arg)
{
    registrar_hilo_activo();
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    LOG_DEBUG(kernel_log, VERDE("=== Interrupt handler iniciado ==="));

    while (kernel_debe_continuar())
    {
        SEM_WAIT(sem_interrupciones);

        LOCK_CON_LOG(mutex_cola_interrupciones);

        if (!kernel_debe_continuar())
        {
            UNLOCK_CON_LOG(mutex_cola_interrupciones);
            break;
        }
        
        t_interrupcion *intr = queue_pop(cola_interrupciones);
        UNLOCK_CON_LOG(mutex_cola_interrupciones);

        if (!intr)
        {
            LOG_ERROR(kernel_log, VERDE("[INTERRUPT] Cola de interrupción vacía"));
            //terminar_kernel(EXIT_FAILURE);
            continue;
        }

        interrumpir_ejecucion(intr->cpu_a_desalojar);

        free(intr);
    }
    
    terminar_hilo();
    return NULL;
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