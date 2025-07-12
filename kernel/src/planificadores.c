#include "../headers/planificadores.h"
#include <sys/time.h>

void iniciar_planificadores()
{
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

double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}