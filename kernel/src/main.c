#include "../headers/kernel.h"
#include <dirent.h>
#include <signal.h>

// Manejador de señales para terminación limpia
void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\nRecibida señal de terminación. Cerrando kernel...\n");
        terminar_kernel(EXIT_SUCCESS);
    }
}

static void listar_configs_kernel()
{
    DIR *d = opendir("kernel");
    if (!d)
    {
        puts("No se pudo abrir directorio kernel/");
        return;
    }

    puts("Archivos .config disponibles en kernel/:");
    struct dirent *de;
    int found = 0;
    while ((de = readdir(d)))
    {
        if (strstr(de->d_name, ".config"))
        {
            printf("  - %s\n", de->d_name);
            found = 1;
        }
    }
    closedir(d);
    if (!found)
        puts("  (ninguno)");
}

int main(int argc, char *argv[])
{

    signal(SIGINT, signal_handler);

    //////////////////////////// Config, log e inicializaciones ////////////////////////////

    if (argc < 3 || argc > 5)
    {
        fprintf(stderr, "Uso: %s <archivo_pseudocodigo> <tamanio_proceso> [kernel.config]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    bool auto_start = (argc == 5 && strcmp(argv[4], "--action") == 0);

    /* ---------------- Ruta del .config ---------------- */
    char ruta_cfg[256] = "kernel/kernel.config"; /* default */
    if (argc >= 4)
    {
        snprintf(ruta_cfg, sizeof(ruta_cfg), "kernel/%s", argv[3]);
    }

    if (access(ruta_cfg, F_OK) == -1)
    {
        fprintf(stderr, "❌ No se encontró %s\n\n", ruta_cfg);
        listar_configs_kernel();
        exit(EXIT_FAILURE);
    }

    /* ---------------- Inicializaciones ---------------- */
    iniciar_sincronizacion_kernel();
    iniciar_config_kernel(ruta_cfg);
    iniciar_logger_kernel();
    iniciar_estados_kernel();
    iniciar_diccionario_tiempos();
    iniciar_diccionario_archivos_por_pcb();

    LOCK_CON_LOG(mutex_planificador_lp);
    iniciar_planificadores();

    if (strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0)
    {
        iniciar_interrupt_handler();
    }

    char *archivo_pseudocodigo = argv[1];
    int tamanio_proceso = atoi(argv[2]);

    //////////////////////////// Conexiones del Kernel ////////////////////////////

    // Servidor de CPU (Dispatch)
    pthread_t *hilo_dispatch = malloc(sizeof(pthread_t));
    if (pthread_create(hilo_dispatch, NULL, hilo_servidor_dispatch, NULL) != 0)
    {
        LOG_ERROR(kernel_log, "Error al crear hilo de servidor Dispatch");
        free(hilo_dispatch);
        terminar_kernel(EXIT_FAILURE);
    }
    LOCK_CON_LOG(mutex_hilos);
    list_add(lista_hilos, hilo_dispatch);
    LOG_DEBUG(kernel_log, "Hilo %d agregado", list_size(lista_hilos));
    UNLOCK_CON_LOG(mutex_hilos);

    // Servidor de CPU (Interrupt)
    pthread_t *hilo_interrupt = malloc(sizeof(pthread_t));
    if (pthread_create(hilo_interrupt, NULL, hilo_servidor_interrupt, NULL) != 0)
    {
        LOG_ERROR(kernel_log, "Error al crear hilo de servidor Interrupt");
        free(hilo_interrupt);
        terminar_kernel(EXIT_FAILURE);
    }
    LOCK_CON_LOG(mutex_hilos);
    list_add(lista_hilos, hilo_interrupt);
    LOG_DEBUG(kernel_log, "Hilo %d agregado", list_size(lista_hilos));
    UNLOCK_CON_LOG(mutex_hilos);

    // Servidor de IO
    pthread_t *hilo_io = malloc(sizeof(pthread_t));
    if (pthread_create(hilo_io, NULL, hilo_servidor_io, NULL) != 0)
    {
        LOG_ERROR(kernel_log, "Error al crear hilo de servidor IO");
        free(hilo_io);
        terminar_kernel(EXIT_FAILURE);
    }
    LOCK_CON_LOG(mutex_hilos);
    list_add(lista_hilos, hilo_io);
    LOG_DEBUG(kernel_log, "Hilo %d agregado", list_size(lista_hilos));
    UNLOCK_CON_LOG(mutex_hilos);

    //////////////////////////// Esperar conexiones minimas ////////////////////////////

    if (!auto_start)
    {
        printf("Esperando conexion con al menos una CPU y una IO...\n");

        while (true)
        {
            LOCK_CON_LOG(mutex_conexiones);
            if (conectado_cpu && conectado_io)
            {
                UNLOCK_CON_LOG(mutex_conexiones);
                break;
            }
            UNLOCK_CON_LOG(mutex_conexiones);
            sleep(1);
        }

        LOG_DEBUG(kernel_log, "CPU y IO conectados. Continuando ejecucion");
    }

    //////////////////////////// Primer proceso ////////////////////////////

    LOG_DEBUG(kernel_log, "Creando proceso inicial:  Archivo: %s, Tamanio: %d", archivo_pseudocodigo, tamanio_proceso);
    INIT_PROC(archivo_pseudocodigo, tamanio_proceso);

    //////////////////////////// Esperar enter ////////////////////////////

    if (auto_start)
    {
        log_info(kernel_log, "Arranque automático (--action): iniciando en 5 s…");
        sleep(5);
        log_info(kernel_log, "Arranque automático (--action): iniciando planificación…");
    }
    else
    {
        puts("\nPresione ENTER para iniciar planificación…");
        int c;
        while ((c = getchar()) != '\n')
        {
            if (c == EOF)
                exit(EXIT_FAILURE);
        }
    }

    LOG_DEBUG(kernel_log, "Kernel ejecutandose. Presione Ctrl+C para terminar.");

    UNLOCK_CON_LOG(mutex_planificador_lp);

    while (!kernel_finalizado)
    {
        sleep(10);
    }

    //////////////////////////// Terminar ////////////////////////////

    terminar_kernel(EXIT_SUCCESS);

    return EXIT_SUCCESS;
}

void iterator(char *value)
{
    LOG_DEBUG(kernel_log, "%s", value);
}
