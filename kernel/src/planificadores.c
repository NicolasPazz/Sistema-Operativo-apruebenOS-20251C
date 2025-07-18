#include "../headers/planificadores.h"
#include <sys/time.h>

void iniciar_planificadores()
{
    pthread_t *hilo_planificador = malloc(sizeof(pthread_t));
    if (pthread_create(hilo_planificador, NULL, planificador_largo_plazo, NULL) != 0)
    {
        LOG_ERROR(kernel_log, "Error al crear hilo para planificador de largo plazo");
        free(hilo_planificador);
        terminar_kernel(EXIT_FAILURE);
    }
    LOCK_CON_LOG(mutex_hilos);
    list_add(lista_hilos, hilo_planificador);
    LOG_DEBUG(kernel_log, "Hilo %d agregado", list_size(lista_hilos));
    UNLOCK_CON_LOG(mutex_hilos);

    pthread_t *hilo_exit = malloc(sizeof(pthread_t));
    if (pthread_create(hilo_exit, NULL, gestionar_exit, NULL) != 0)
    {
        LOG_ERROR(kernel_log, "Error al crear hilo para gestionar procesos en EXIT");
        free(hilo_exit);
        terminar_kernel(EXIT_FAILURE);
    }
    LOCK_CON_LOG(mutex_hilos);
    list_add(lista_hilos, hilo_exit);
    LOG_DEBUG(kernel_log, "Hilo %d agregado", list_size(lista_hilos));
    UNLOCK_CON_LOG(mutex_hilos);

    pthread_t *hilo_planificador_cp = malloc(sizeof(pthread_t));
    if (pthread_create(hilo_planificador_cp, NULL, planificador_corto_plazo, NULL) != 0)
    {
        LOG_ERROR(kernel_log, "Error al crear hilo para planificador de corto plazo");
        free(hilo_planificador_cp);
        terminar_kernel(EXIT_FAILURE);
    }
    LOCK_CON_LOG(mutex_hilos);
    list_add(lista_hilos, hilo_planificador_cp);
    LOG_DEBUG(kernel_log, "Hilo %d agregado", list_size(lista_hilos));
    UNLOCK_CON_LOG(mutex_hilos);

    pthread_t *hilo_rechazados = malloc(sizeof(pthread_t));
    if (pthread_create(hilo_rechazados, NULL, verificar_procesos_rechazados, NULL) != 0)
    {
        LOG_ERROR(kernel_log, "[PLANI LP] [EXIT] Error al crear hilo para verificar procesos rechazados");
        free(hilo_rechazados);
        terminar_kernel(EXIT_FAILURE);
    }
    LOCK_CON_LOG(mutex_hilos);
    list_add(lista_hilos, hilo_rechazados);
    LOG_DEBUG(kernel_log, "Hilo %d agregado", list_size(lista_hilos));
    UNLOCK_CON_LOG(mutex_hilos);
}

// AUXILIARES

double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

t_pcb *elegir_por_fifo(t_list *cola_a_utilizar)
{
    LOG_DEBUG(kernel_log, "ELIGIENDO POR FIFO");

    if (list_is_empty(cola_a_utilizar))
    {
        LOG_ERROR(kernel_log, "[FIFO] cola_a_utilizar vacía");
        return NULL;
    }

    LOG_DEBUG(kernel_log, "[FIFO] seleccionando el primer PCB de la cola");
    return (t_pcb *)list_get(cola_a_utilizar, 0);
}