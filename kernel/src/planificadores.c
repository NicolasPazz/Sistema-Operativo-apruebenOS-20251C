#include "../headers/planificadores.h"
#include <sys/time.h>

pthread_mutex_t mutex_planificador_lp;
pthread_cond_t cond_planificador_lp;
estado_planificador estado_planificador_lp = STOP;

void iniciar_planificadores(void)
{
    // Inicializar mutex y condición
    pthread_mutex_init(&mutex_planificador_lp, NULL);
    pthread_cond_init(&cond_planificador_lp, NULL);
    estado_planificador_lp = STOP;

    // Crear hilo del planificador
    pthread_t hilo_planificador;
    pthread_create(&hilo_planificador, NULL, planificador_largo_plazo, NULL);
    pthread_detach(hilo_planificador);

    log_trace(kernel_log, "Planificador de largo plazo iniciado con algoritmo: %s", ALGORITMO_INGRESO_A_READY);

    pthread_t hilo_exit;
    if (pthread_create(&hilo_exit, NULL, gestionar_exit, NULL) != 0)
    {
        log_error(kernel_log, "Error al crear hilo para gestionar procesos en EXIT");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_exit);

    // Iniciar planificador de corto plazo
    pthread_t hilo_planificador_cp;
    if (pthread_create(&hilo_planificador_cp, NULL, planificador_corto_plazo, NULL) != 0)
    {
        log_error(kernel_log, "Error al crear hilo para planificador de corto plazo");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_planificador_cp);
    log_trace(kernel_log, "Planificador de corto plazo iniciado correctamente");
}

// AUXILIARES

t_pcb *elegir_por_fifo()
{
    log_trace(kernel_log, "PLANIFICANDO FIFO");
    log_debug(kernel_log, "FIFO: esperando mutex_cola_ready para elegir proceso FIFO");
    pthread_mutex_lock(&mutex_cola_ready);
    log_debug(kernel_log, "FIFO: bloqueando mutex_cola_ready para elegir proceso FIFO");

    // Se elegira al siguiente proceso a ejecutar segun su orden de llegada a READY.
    if (list_is_empty(cola_ready))
    {
        pthread_mutex_unlock(&mutex_cola_ready);
        log_error(kernel_log, "FIFO: cola_ready vacía");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    t_pcb *pcb_fifo = (t_pcb *)list_get(cola_ready, 0);
    pthread_mutex_unlock(&mutex_cola_ready);

    return pcb_fifo;
}

double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}