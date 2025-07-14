#include "../headers/planificadores.h"

void iniciar_interrupt_handler()
{
    pthread_t hilo_interrupt_handler;
    if (pthread_create(&hilo_interrupt_handler, NULL, interrupt_handler, NULL) != 0)
    {
        log_error(kernel_log, "[INTERRUPT] Error al crear hilo para manejar interrupciones");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_interrupt_handler);
    log_trace(kernel_log, "[INTERRUPT] Hilo de manejo de interrupciones iniciado correctamente");
}

void *interrupt_handler(void *arg)
{
    log_debug(kernel_log, VERDE("=== Interrupt handler iniciado ==="));

    while (1)
    {
        log_trace(kernel_log, "[INTERRUPT] semaforo INTERRUPCIONES disminuido");
        sem_wait(&sem_interrupciones);

        log_trace(kernel_log, "[INTERRUPT] esperando mutex_cola_interrupciones");
        pthread_mutex_lock(&mutex_cola_interrupciones);
        log_trace(kernel_log, "[INTERRUPT] bloqueando mutex_cola_interrupciones");
        t_interrupcion *intr = queue_pop(cola_interrupciones);
        pthread_mutex_unlock(&mutex_cola_interrupciones);

        if (!intr)
        {
            log_error(kernel_log, VERDE("[INTERRUPT] Cola de interrupción vacía"));
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        interrumpir_ejecucion(intr->cpu_a_desalojar);

        free(intr);
    }
}

bool interrupt(cpu *cpu_a_desalojar)
{
    t_interrupcion *nueva = malloc(sizeof(t_interrupcion));
    nueva->cpu_a_desalojar = cpu_a_desalojar;

    log_debug(kernel_log, "[INTERRUPT] esperando mutex_cola_interrupciones para encolar interrupción");
    pthread_mutex_lock(&mutex_cola_interrupciones);
    log_debug(kernel_log, "[INTERRUPT] bloqueando mutex_cola_interrupciones para encolar interrupción");

    queue_push(cola_interrupciones, nueva);
    pthread_mutex_unlock(&mutex_cola_interrupciones);
    log_trace(kernel_log, "[INTERRUPT]: Interrupción encolada para desalojar CPU %d (fd=%d)", cpu_a_desalojar->id, cpu_a_desalojar->fd);

    sem_post(&sem_interrupciones);
    log_debug(kernel_log, "[INTERRUPT]: semaforo INTERRUPCIONES aumentado para procesar interrupción");
}