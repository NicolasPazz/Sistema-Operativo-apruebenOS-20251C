#include "../headers/kernel.h"
#include "../headers/CPUKernel.h"
#include "../headers/syscalls.h"
#include <libgen.h>

/////////////////////////////// Declaracion de variables globales ///////////////////////////////

// Logger
t_log *kernel_log = NULL;

// Hashmap de cronometros por PCB
t_dictionary *tiempos_por_pid = NULL;
t_dictionary *archivo_por_pcb = NULL;

// Sockets
int fd_kernel_dispatch;
int fd_interrupt;
int fd_kernel_io;

static pthread_mutex_t fin_mutex = PTHREAD_MUTEX_INITIALIZER;
bool kernel_finalizado = false;

// Config
t_config *kernel_config = NULL;
char *IP_MEMORIA = NULL;
char *PUERTO_MEMORIA = NULL;
char *PUERTO_ESCUCHA_DISPATCH = NULL;
char *PUERTO_ESCUCHA_INTERRUPT = NULL;
char *PUERTO_ESCUCHA_IO = NULL;
char *ALGORITMO_CORTO_PLAZO = NULL;
char *ALGORITMO_INGRESO_A_READY = NULL;
double ALFA;
double TIEMPO_SUSPENSION;
double ESTIMACION_INICIAL;
char *LOG_LEVEL = NULL;

// Colas de Estados
t_list *cola_new = NULL;
t_list *cola_ready = NULL;
t_list *cola_running = NULL;
t_list *cola_blocked = NULL;
t_list *cola_susp_ready = NULL;
t_list *cola_susp_blocked = NULL;
t_list *cola_exit = NULL;
t_list *cola_procesos = NULL; // Cola con TODOS los procesos sin importar el estado (Procesos totales del sistema)
t_list *pcbs_bloqueados_por_dump_memory = NULL;
t_list *pcbs_esperando_io = NULL;
t_queue *cola_interrupciones = NULL;

// Listas y semaforos de CPUs y IOs conectadas
t_list *lista_cpus = NULL;
int cpu_libre = 0; // Contador de CPUs libres
pthread_mutex_t mutex_lista_cpus;
t_list *lista_ios = NULL;
pthread_mutex_t mutex_ios;

// Conexiones minimas
bool conectado_cpu = false;
bool conectado_io = false;
pthread_mutex_t mutex_conexiones;
t_list *lista_hilos;
t_list *lista_sockets;
pthread_mutex_t mutex_hilos;
pthread_mutex_t mutex_sockets;

// Semaforos de planificacion
pthread_mutex_t mutex_cola_new;
pthread_mutex_t mutex_cola_susp_ready;
pthread_mutex_t mutex_cola_susp_blocked;
pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_running;
pthread_mutex_t mutex_cola_blocked;
pthread_mutex_t mutex_cola_exit;
pthread_mutex_t mutex_cola_procesos;
pthread_mutex_t mutex_pcbs_esperando_io;
pthread_mutex_t mutex_cola_interrupciones;
pthread_mutex_t mutex_planificador_lp;
pthread_mutex_t mutex_procesos_rechazados;
pthread_mutex_t mutex_inicializacion_procesos;
sem_t sem_proceso_a_new;
sem_t sem_proceso_a_susp_ready;
sem_t sem_proceso_a_susp_blocked;
sem_t sem_proceso_a_ready;
sem_t sem_proceso_a_running;
sem_t sem_proceso_a_blocked;
sem_t sem_proceso_a_exit;
sem_t sem_cpu_disponible;
sem_t sem_planificador_cp;
sem_t sem_interrupciones;
sem_t sem_procesos_rechazados;

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                       INICIALIZACIONES                                       //
//////////////////////////////////////////////////////////////////////////////////////////////////

void iniciar_config_kernel(const char *path_cfg)
{
    kernel_config = iniciar_config((char *)path_cfg);

    IP_MEMORIA = config_get_string_value(kernel_config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_string_value(kernel_config, "PUERTO_MEMORIA");
    PUERTO_ESCUCHA_DISPATCH = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_DISPATCH");
    PUERTO_ESCUCHA_INTERRUPT = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_INTERRUPT");
    PUERTO_ESCUCHA_IO = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_IO");
    ALGORITMO_CORTO_PLAZO = config_get_string_value(kernel_config, "ALGORITMO_CORTO_PLAZO");
    ALGORITMO_INGRESO_A_READY = config_get_string_value(kernel_config, "ALGORITMO_INGRESO_A_READY");
    ALFA = config_get_double_value(kernel_config, "ALFA");
    ESTIMACION_INICIAL = config_get_double_value(kernel_config, "ESTIMACION_INICIAL");
    TIEMPO_SUSPENSION = config_get_double_value(kernel_config, "TIEMPO_SUSPENSION");
    LOG_LEVEL = config_get_string_value(kernel_config, "LOG_LEVEL");

    if (!IP_MEMORIA || !PUERTO_MEMORIA || !PUERTO_ESCUCHA_DISPATCH ||
        !PUERTO_ESCUCHA_INTERRUPT || !PUERTO_ESCUCHA_IO ||
        !ALGORITMO_CORTO_PLAZO || !ALGORITMO_INGRESO_A_READY || !LOG_LEVEL)
    {
        fprintf(stderr, "iniciar_config_kernel: Faltan campos obligatorios en %s\n", path_cfg);
        config_destroy(kernel_config);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("        Config leída: %s\n", path_cfg);
        printf("    IP_MEMORIA                 : %s\n", IP_MEMORIA);
        printf("    PUERTO_MEMORIA             : %s\n", PUERTO_MEMORIA);
        printf("    PUERTO_ESCUCHA_DISPATCH    : %s\n", PUERTO_ESCUCHA_DISPATCH);
        printf("    PUERTO_ESCUCHA_INTERRUPT   : %s\n", PUERTO_ESCUCHA_INTERRUPT);
        printf("    PUERTO_ESCUCHA_IO          : %s\n", PUERTO_ESCUCHA_IO);
        printf("    ALGORITMO_CORTO_PLAZO      : %s\n", ALGORITMO_CORTO_PLAZO);
        printf("    ALGORITMO_INGRESO_A_READY  : %s\n", ALGORITMO_INGRESO_A_READY);
        printf("    ALFA                       : %.3f\n", ALFA);
        printf("    ESTIMACION_INICIAL         : %.3f\n", ESTIMACION_INICIAL);
        printf("    TIEMPO_SUSPENSION          : %.3f\n", TIEMPO_SUSPENSION);
        printf("    LOG_LEVEL                  : %s\n\n", LOG_LEVEL);
    }
}

void iniciar_logger_kernel()
{
    kernel_log = iniciar_logger("kernel/kernel.log", "KERNEL", 1, log_level_from_string(LOG_LEVEL));
    LOG_DEBUG(kernel_log, "Kernel log iniciado correctamente!");
}

void iniciar_estados_kernel()
{
    cola_new = list_create();
    cola_ready = list_create();
    cola_running = list_create();
    cola_blocked = list_create();
    cola_susp_ready = list_create();
    cola_susp_blocked = list_create();
    cola_exit = list_create();
    cola_procesos = list_create();
    pcbs_bloqueados_por_dump_memory = list_create();
    pcbs_esperando_io = list_create();
}

void iniciar_sincronizacion_kernel()
{
    pthread_mutex_init(&mutex_lista_cpus, NULL);
    pthread_mutex_init(&mutex_ios, NULL);
    pthread_mutex_init(&mutex_conexiones, NULL);
    pthread_mutex_init(&mutex_planificador_lp, NULL);
    pthread_mutex_init(&mutex_hilos, NULL);
    pthread_mutex_init(&mutex_sockets, NULL);

    pthread_mutex_init(&mutex_cola_new, NULL);
    pthread_mutex_init(&mutex_cola_susp_ready, NULL);
    pthread_mutex_init(&mutex_cola_susp_blocked, NULL);
    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_running, NULL);
    pthread_mutex_init(&mutex_cola_blocked, NULL);
    pthread_mutex_init(&mutex_cola_exit, NULL);
    pthread_mutex_init(&mutex_cola_procesos, NULL);

    pthread_mutex_init(&mutex_pcbs_esperando_io, NULL);
    pthread_mutex_init(&mutex_cola_interrupciones, NULL);
    pthread_mutex_init(&mutex_procesos_rechazados, NULL);
    pthread_mutex_init(&mutex_inicializacion_procesos, NULL);

    sem_init(&sem_proceso_a_new, 0, 0);
    sem_init(&sem_proceso_a_susp_ready, 0, 0);
    sem_init(&sem_proceso_a_susp_blocked, 0, 0);
    sem_init(&sem_proceso_a_ready, 0, 0);
    sem_init(&sem_proceso_a_running, 0, 0);
    sem_init(&sem_proceso_a_blocked, 0, 0);
    sem_init(&sem_proceso_a_exit, 0, 0);
    sem_init(&sem_cpu_disponible, 0, 0);
    sem_init(&sem_planificador_cp, 0, 0);
    sem_init(&sem_interrupciones, 0, 0);
    sem_init(&sem_procesos_rechazados, 0, 0);

    lista_cpus = list_create();
    lista_ios = list_create();
    lista_hilos = list_create();
    lista_sockets = list_create();
    cola_interrupciones = queue_create();

    conectado_cpu = false;
    conectado_io = false;
}

void iniciar_diccionario_tiempos()
{
    tiempos_por_pid = dictionary_create();
}

void iniciar_diccionario_archivos_por_pcb()
{
    archivo_por_pcb = dictionary_create();
}

void terminar_hilo(void)
{
    pthread_t self = pthread_self();

    LOCK_CON_LOG(mutex_hilos);
    for (int i = 0; i < list_size(lista_hilos); i++) {
        pthread_t *hilo = list_get(lista_hilos, i);
        if (hilo == NULL)         // posición vacía: seguir
            continue;

        if (pthread_equal(*hilo, self)) {
            free(hilo);                          // evita fuga
            list_replace(lista_hilos, i, NULL);  // marca la posición como vacía
            break;
        }
    }
    UNLOCK_CON_LOG(mutex_hilos);
}

static void destruir_cpu(void *elem)
{
    if (!elem)
        return;
    cpu *c = elem;
    close(c->fd);
    free(c);
}

static void destruir_io(void *elem)
{
    if (!elem)
        return;
    io *d = elem;
    close(d->fd);
    free(d->nombre);
    free(d);
}

static void destruir_pcb(void *elem)
{
    if (!elem)
        return;
    t_pcb *pcb = elem;
    free(pcb->path);
    free(pcb);
}

static void destruir_pcb_io(void *elem)
{
    free(elem);
}

static void destruir_pcb_dump(void *elem)
{
    free(elem);
}

void terminar_kernel(int code)
{
    pthread_mutex_lock(&fin_mutex);
    if (kernel_finalizado)
    {
        pthread_mutex_unlock(&fin_mutex);
        return;
    }
    kernel_finalizado = true;
    pthread_mutex_unlock(&fin_mutex);

    SEM_POST(sem_interrupciones);
    SEM_POST(sem_proceso_a_new);
    SEM_POST(sem_proceso_a_exit);
    SEM_POST(sem_planificador_cp);
    SEM_POST(sem_procesos_rechazados);

    SEM_POST(sem_proceso_a_susp_ready);
    SEM_POST(sem_proceso_a_susp_blocked);
    SEM_POST(sem_proceso_a_ready);
    SEM_POST(sem_proceso_a_running);
    SEM_POST(sem_proceso_a_blocked);
    SEM_POST(sem_cpu_disponible);   

    pthread_t self = pthread_self();
    LOCK_CON_LOG(mutex_hilos);
    for (int i = 0; i < list_size(lista_hilos); i++)
    {
        pthread_t *h = list_get(lista_hilos, i);
        if (!h)
            continue;
        if (!pthread_equal(*h, self))
        {
            pthread_cancel(*h);
            pthread_join(*h, NULL);
        }
    }
    UNLOCK_CON_LOG(mutex_hilos);

    LOCK_CON_LOG(mutex_sockets);
    close(fd_kernel_io);
    close(fd_kernel_dispatch);
    close(fd_interrupt);
    for (int i = 0; i < list_size(lista_sockets); i++)
    {
        int fd = (int)(intptr_t)list_get(lista_sockets, i);
        close(fd);
        LOG_DEBUG(kernel_log, "Socket cerrado: fd=%d", fd);
    }
    list_destroy(lista_sockets);
    UNLOCK_CON_LOG(mutex_sockets);
    pthread_mutex_destroy(&mutex_sockets);

    LOG_DEBUG(kernel_log, "Destruyendo tiempos por PID y archivos por PCB");
    dictionary_destroy_and_destroy_elements(tiempos_por_pid, (void *)temporal_destroy);
    dictionary_destroy_and_destroy_elements(archivo_por_pcb, free);

    LOCK_CON_LOG(mutex_cola_procesos);
    list_destroy_and_destroy_elements(cola_procesos, destruir_pcb);
    UNLOCK_CON_LOG(mutex_cola_procesos);
    pthread_mutex_destroy(&mutex_cola_procesos);

    LOCK_CON_LOG(mutex_cola_new);
    list_destroy(cola_new);
    sem_destroy(&sem_proceso_a_new);

    UNLOCK_CON_LOG(mutex_cola_new);
    pthread_mutex_destroy(&mutex_cola_new);

    LOCK_CON_LOG(mutex_cola_susp_blocked);
    list_destroy(cola_susp_blocked);
    sem_destroy(&sem_proceso_a_susp_blocked);

    UNLOCK_CON_LOG(mutex_cola_susp_blocked);
    pthread_mutex_destroy(&mutex_cola_susp_blocked);

    LOCK_CON_LOG(mutex_cola_susp_ready);
    list_destroy(cola_susp_ready);
    sem_destroy(&sem_proceso_a_susp_ready);

    UNLOCK_CON_LOG(mutex_cola_susp_ready);
    pthread_mutex_destroy(&mutex_cola_susp_ready);

    LOCK_CON_LOG(mutex_cola_ready);
    list_destroy(cola_ready);
    sem_destroy(&sem_proceso_a_ready);
    sem_destroy(&sem_planificador_cp);
    UNLOCK_CON_LOG(mutex_cola_ready);
    pthread_mutex_destroy(&mutex_cola_ready);

    LOCK_CON_LOG(mutex_cola_blocked);
    list_destroy(cola_blocked);
    sem_destroy(&sem_proceso_a_blocked);

    UNLOCK_CON_LOG(mutex_cola_blocked);
    pthread_mutex_destroy(&mutex_cola_blocked);

    LOCK_CON_LOG(mutex_cola_running);
    list_destroy(cola_running);
    sem_destroy(&sem_proceso_a_running);

    UNLOCK_CON_LOG(mutex_cola_running);
    pthread_mutex_destroy(&mutex_cola_running);

    LOCK_CON_LOG(mutex_cola_exit);
    list_destroy(cola_exit);
    sem_destroy(&sem_proceso_a_exit);

    UNLOCK_CON_LOG(mutex_cola_exit);
    pthread_mutex_destroy(&mutex_cola_exit);

    LOCK_CON_LOG(mutex_pcbs_esperando_io);
    list_destroy_and_destroy_elements(pcbs_esperando_io, destruir_pcb_io);
    UNLOCK_CON_LOG(mutex_pcbs_esperando_io);
    pthread_mutex_destroy(&mutex_pcbs_esperando_io);

    LOCK_CON_LOG(mutex_lista_cpus);
    list_destroy_and_destroy_elements(lista_cpus, destruir_cpu);
    sem_destroy(&sem_cpu_disponible);
    UNLOCK_CON_LOG(mutex_lista_cpus);
    pthread_mutex_destroy(&mutex_lista_cpus);

    LOCK_CON_LOG(mutex_ios);
    list_destroy_and_destroy_elements(lista_ios, destruir_io);
    UNLOCK_CON_LOG(mutex_ios);
    pthread_mutex_destroy(&mutex_ios);

    LOCK_CON_LOG(mutex_cola_interrupciones);
    queue_destroy_and_destroy_elements(cola_interrupciones, free);
    sem_destroy(&sem_interrupciones);
    UNLOCK_CON_LOG(mutex_cola_interrupciones);
    pthread_mutex_destroy(&mutex_cola_interrupciones);

    pthread_mutex_destroy(&mutex_procesos_rechazados);
    sem_destroy(&sem_procesos_rechazados);

    pthread_mutex_destroy(&mutex_inicializacion_procesos);

    list_destroy_and_destroy_elements(pcbs_bloqueados_por_dump_memory, destruir_pcb_dump);

    pthread_mutex_destroy(&mutex_conexiones);

    UNLOCK_CON_LOG(mutex_planificador_lp);
    pthread_mutex_destroy(&mutex_planificador_lp);

    // Liberar correctamente los pthread_t* mallocados
    /*LOCK_CON_LOG(mutex_hilos);
    list_destroy_and_destroy_elements(lista_hilos, free);
    UNLOCK_CON_LOG(mutex_hilos);
    pthread_mutex_destroy(&mutex_hilos);*/

    if (code)
    {
        LOG_ERROR(kernel_log, "Kernel finalizado con errores. Código de salida: %d", code);
    }
    else
    {
        log_info(kernel_log, "Kernel finalizado correctamente.");
    }

    log_destroy(kernel_log);
    config_destroy(kernel_config);

    exit(code);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                         CPU DISPATCH                                         //
//////////////////////////////////////////////////////////////////////////////////////////////////

void *hilo_servidor_dispatch(void *_)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    fd_kernel_dispatch = iniciar_servidor(PUERTO_ESCUCHA_DISPATCH, kernel_log, "Kernel Dispatch");

    while (!kernel_finalizado)
    {
        int fd_cpu_dispatch = esperar_cliente(fd_kernel_dispatch, kernel_log);
        if (fd_cpu_dispatch == -1)
        {
            LOG_ERROR(kernel_log, "Error al recibir cliente");
            continue;
        }
        LOCK_CON_LOG(mutex_sockets);
        list_add(lista_sockets, (void *)(intptr_t)fd_cpu_dispatch);
        UNLOCK_CON_LOG(mutex_sockets);
        if (!validar_handshake(fd_cpu_dispatch, HANDSHAKE_CPU_KERNEL_DISPATCH, kernel_log))
        {
            close(fd_cpu_dispatch);
            continue;
        }

        int id_cpu;
        if (recv(fd_cpu_dispatch, &id_cpu, sizeof(int), 0) <= 0)
        {
            LOG_ERROR(kernel_log, "Error al recibir ID de CPU desde Dispatch");
            close(fd_cpu_dispatch);
            continue;
        }

        cpu *nueva_cpu = malloc(sizeof(cpu));
        nueva_cpu->fd = fd_cpu_dispatch;
        nueva_cpu->id = id_cpu;
        nueva_cpu->tipo_conexion = CPU_DISPATCH;
        nueva_cpu->pid = -1;
        nueva_cpu->instruccion_actual = -1;

        LOCK_CON_LOG(mutex_lista_cpus);
        list_add(lista_cpus, nueva_cpu);
        cpu_libre++;
        UNLOCK_CON_LOG(mutex_lista_cpus);

        int *arg = malloc(sizeof(int));
        *arg = fd_cpu_dispatch;
        pthread_t *hilo = malloc(sizeof(pthread_t));
        if (pthread_create(hilo, NULL, atender_cpu_dispatch, arg) != 0)
        {
            LOG_ERROR(kernel_log, "Error al crear hilo para atender CPU Dispatch (fd=%d)", fd_cpu_dispatch);
            LOCK_CON_LOG(mutex_lista_cpus);
            list_remove_element(lista_cpus, nueva_cpu);
            cpu_libre--;
            UNLOCK_CON_LOG(mutex_lista_cpus);
            close(fd_cpu_dispatch);
            free(nueva_cpu);
            free(arg);
            free(hilo);
            continue;
        }
        LOCK_CON_LOG(mutex_hilos);
        list_add(lista_hilos, hilo);
        LOG_DEBUG(kernel_log, "Hilo %d agregado para atender CPU Dispatch (fd=%d)", *hilo, fd_cpu_dispatch);
        UNLOCK_CON_LOG(mutex_hilos);

        SEM_POST(sem_planificador_cp);
        LOG_DEBUG(kernel_log, "[PLANI CP] Replanificación solicitada por nueva CPU Dispatch (fd=%d, ID=%d)", fd_cpu_dispatch, id_cpu);

        LOCK_CON_LOG(mutex_conexiones);
        conectado_cpu = true;
        UNLOCK_CON_LOG(mutex_conexiones);

        LOG_DEBUG(kernel_log, "HANDSHAKE_CPU_KERNEL_DISPATCH: CPU conectada exitosamente a Dispatch (fd=%d), ID=%d", nueva_cpu->fd, nueva_cpu->id);
    }

    terminar_hilo();
    return NULL;
}

void *atender_cpu_dispatch(void *arg)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    int fd_cpu_dispatch = *(int *)arg;
    free(arg);

    if (fd_cpu_dispatch < 0)
    {
        LOG_ERROR(kernel_log, "[SERVIDOR DISPATCH] Error en el fd de la CPU Dispatch: %d", fd_cpu_dispatch);
        terminar_hilo();
        return NULL;
    }

    op_code cop;
    while ((cop = recibir_operacion(fd_cpu_dispatch)) != -1)
    {
        LOCK_CON_LOG(mutex_lista_cpus);

        cpu *cpu_actual = get_cpu_from_fd(fd_cpu_dispatch);

        if (!cpu_actual)
        {
            LOG_ERROR(kernel_log, "[SERVIDOR DISPATCH] No se encontró CPU asociada al fd=%d", fd_cpu_dispatch);
            close(fd_cpu_dispatch);
            terminar_hilo();
            return NULL;
        }

        cpu_actual->instruccion_actual = cop;
        int pid = cpu_actual->pid;

        UNLOCK_CON_LOG(mutex_lista_cpus);

        LOG_DEBUG(kernel_log, "[SERVIDOR DISPATCH] CPU ID=%d está procesando operación %d para pid %d", cpu_actual->id, cop, pid);

        switch (cop)
        {
        case INIT_PROC_OP:
        {
            log_info(kernel_log, VERDE("## (%d) Solicitó syscall: ") ROJO("INIT_PROC"), pid);
            LOG_DEBUG(kernel_log, "[SERVIDOR DISPATCH] INIT_PROC_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);

            t_list *parametros_init_proc = recibir_contenido_paquete(fd_cpu_dispatch);
            if (!parametros_init_proc || list_size(parametros_init_proc) < 2)
            {
                LOG_ERROR(kernel_log, "[SERVIDOR DISPATCH] Error al recibir parámetros para INIT_PROC_OP desde CPU Dispatch");
                terminar_kernel(EXIT_FAILURE);
            }

            char *nombre = (char *)list_get(parametros_init_proc, 0);
            int size = *(int *)list_get(parametros_init_proc, 1);
            LOG_DEBUG(kernel_log, "[SERVIDOR DISPATCH] INIT_PROC_OP recibido de CPU Dispatch (fd=%d) con nombre '%s', tamaño %d", fd_cpu_dispatch, nombre, size);

            INIT_PROC(nombre, size);

            list_destroy_and_destroy_elements(parametros_init_proc, free);
            break;
        }

        case IO_OP:
        {
            log_info(kernel_log, VERDE("## (%d) Solicitó syscall: ") ROJO("IO"), pid);

            t_list *parametros_io = recibir_contenido_paquete(fd_cpu_dispatch);
            if (!parametros_io || list_size(parametros_io) < 3)
            {
                LOG_ERROR(kernel_log, "[SERVIDOR DISPATCH] Error al recibir parámetros para IO_OP desde CPU Dispatch");
                terminar_kernel(EXIT_FAILURE);
            }

            char *nombre_IO = (char *)list_get(parametros_io, 0);
            int cant_tiempo = *(int *)list_get(parametros_io, 1);
            int PC = *(int *)list_get(parametros_io, 2);

            LOG_DEBUG(kernel_log, "[SERVIDOR DISPATCH] IO_OP recibido de CPU Dispatch (fd=%d) con nombre '%s', tiempo %d ms, PC %d", fd_cpu_dispatch, nombre_IO, cant_tiempo, PC);

            t_pcb *pcb_a_io = buscar_pcb(pid);
            pcb_a_io->PC = PC;

            IO(nombre_IO, cant_tiempo, pcb_a_io);

            liberar_cpu(cpu_actual);

            list_destroy_and_destroy_elements(parametros_io, free);
            break;
        }

        case EXIT_OP:
            log_info(kernel_log, VERDE("## (%d) Solicitó syscall: ") ROJO("EXIT"), pid);
            LOG_DEBUG(kernel_log, "[SERVIDOR DISPATCH] EXIT_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);

            t_pcb *pcb_a_finalizar = buscar_pcb(pid);
            cambiar_estado_pcb_mutex(pcb_a_finalizar, EXIT_ESTADO);

            liberar_cpu(cpu_actual);

            break;

        case DUMP_MEMORY_OP:
            log_info(kernel_log, VERDE("## (%d) Solicitó syscall: ") ROJO("DUMP_MEMORY"), pid);
            LOG_DEBUG(kernel_log, "[SERVIDOR DISPATCH] DUMP_MEMORY_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);

            t_list *parametros_dump = recibir_contenido_paquete(fd_cpu_dispatch);
            if (!parametros_dump || list_size(parametros_dump) < 2)
            {
                LOG_ERROR(kernel_log, "[SERVIDOR DISPATCH] DUMP_MEMORY_OP: Error al recibir parámetros desde CPU Dispatch. Esperados: 2, recibidos: %d", parametros_dump ? list_size(parametros_dump) : 0);
                if (parametros_dump)
                    list_destroy_and_destroy_elements(parametros_dump, free);
                terminar_kernel(EXIT_FAILURE);
            }

            int PID = *(int *)list_get(parametros_dump, 0);
            int PC = *(int *)list_get(parametros_dump, 1);

            if (PID != pid)
            {
                LOG_ERROR(kernel_log, "[SERVIDOR DISPATCH] DUMP_MEMORY_OP: PID recibido (%d) no coincide con PID de la CPU Dispatch (%d)", PID, pid);
                list_destroy_and_destroy_elements(parametros_dump, free);
                terminar_kernel(EXIT_FAILURE);
            }

            t_pcb *pcb_dump = buscar_pcb(PID);
            pcb_dump->PC = PC;
            cambiar_estado_pcb_mutex(pcb_dump, BLOCKED);

            liberar_cpu(cpu_actual);

            DUMP_MEMORY(pcb_dump);

            list_destroy_and_destroy_elements(parametros_dump, free);
            break;

        default:
            LOG_ERROR(kernel_log, "[SERVIDOR DISPATCH] (%d) Código op desconocido recibido de Dispatch fd %d: %d", pid, fd_cpu_dispatch, cop);
            break;
        }

        // Limpiar la instrucción actual de la CPU
        LOCK_CON_LOG(mutex_lista_cpus);
        cpu_actual->instruccion_actual = -1; // Valor inválido para indicar que está libre
        UNLOCK_CON_LOG(mutex_lista_cpus);
    }

    LOG_DEBUG(kernel_log, "[SERVIDOR DISPATCH] CPU Dispatch desconectada (fd=%d)", fd_cpu_dispatch);

    LOCK_CON_LOG(mutex_lista_cpus);
    cpu_libre--;
    cpu *cpu_eliminada = buscar_y_remover_cpu_por_fd(fd_cpu_dispatch);

    if (!cpu_eliminada)
    {
        LOG_DEBUG(kernel_log, "[SERVIDOR DISPATCH] No se encontró CPU asociada al fd=%d al desconectar", fd_cpu_dispatch);
        UNLOCK_CON_LOG(mutex_lista_cpus);
        terminar_hilo();
        return NULL;
    }

    free(cpu_eliminada);
    close(fd_cpu_dispatch);

    UNLOCK_CON_LOG(mutex_lista_cpus);

    terminar_hilo();
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                         CPU INTERRUPT                                        //
//////////////////////////////////////////////////////////////////////////////////////////////////

void *hilo_servidor_interrupt(void *_)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    fd_interrupt = iniciar_servidor(PUERTO_ESCUCHA_INTERRUPT, kernel_log, "Kernel Interrupt");

    while (!kernel_finalizado)
    {
        int fd_cpu_interrupt = esperar_cliente(fd_interrupt, kernel_log);
        if (fd_cpu_interrupt == -1)
        {
            LOG_ERROR(kernel_log, "[SERVIDOR INTERRUPT] Error al recibir cliente");
            continue;
        }
        LOCK_CON_LOG(mutex_sockets);
        list_add(lista_sockets, (void *)(intptr_t)fd_cpu_interrupt);
        UNLOCK_CON_LOG(mutex_sockets);
        if (!validar_handshake(fd_cpu_interrupt, HANDSHAKE_CPU_KERNEL_INTERRUPT, kernel_log))
        {
            close(fd_cpu_interrupt);
            continue;
        }

        int id_cpu;
        if (recv(fd_cpu_interrupt, &id_cpu, sizeof(int), 0) <= 0)
        {
            LOG_DEBUG(kernel_log, "[SERVIDOR INTERRUPT] CPU desconectada en canal INTERRUPT (fd=%d)", fd_cpu_interrupt);
            LOCK_CON_LOG(mutex_lista_cpus);
            cpu *cpu_eliminada = buscar_y_remover_cpu_por_fd(fd_cpu_interrupt);

            if (!cpu_eliminada)
            {
                LOG_ERROR(kernel_log, "[SERVIDOR INTERRUPT] No se encontró CPU asociada al fd=%d al desconectar", fd_cpu_interrupt);
                UNLOCK_CON_LOG(mutex_lista_cpus);
                terminar_hilo();
                return NULL;
            }

            free(cpu_eliminada);
            close(fd_cpu_interrupt);

            UNLOCK_CON_LOG(mutex_lista_cpus);
            continue;
        }

        cpu *nueva_cpu = malloc(sizeof(cpu));
        nueva_cpu->fd = fd_cpu_interrupt;
        nueva_cpu->id = id_cpu;
        nueva_cpu->tipo_conexion = CPU_INTERRUPT;
        nueva_cpu->pid = -1;
        nueva_cpu->instruccion_actual = -1;

        LOCK_CON_LOG(mutex_lista_cpus);

        list_add(lista_cpus, nueva_cpu);
        UNLOCK_CON_LOG(mutex_lista_cpus);

        LOG_DEBUG(kernel_log, "[SERVIDOR INTERRUPT] CPU conectada exitosamente a Interrupt (fd=%d), ID=%d", nueva_cpu->fd, nueva_cpu->id);
    }

    terminar_hilo();
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                              IO                                              //
//////////////////////////////////////////////////////////////////////////////////////////////////

void *hilo_servidor_io(void *_)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    fd_kernel_io = iniciar_servidor(PUERTO_ESCUCHA_IO, kernel_log, "Kernel IO");

    while (!kernel_finalizado)
    {
        int fd_io = esperar_cliente(fd_kernel_io, kernel_log);
        if (fd_io == -1)
        {
            LOG_ERROR(kernel_log, "[SERVIDOR IO] Error al recibir cliente");
            terminar_kernel(EXIT_FAILURE);
        }
        LOCK_CON_LOG(mutex_sockets);
        list_add(lista_sockets, (void *)(intptr_t)fd_io);
        UNLOCK_CON_LOG(mutex_sockets);
        if (!validar_handshake(fd_io, HANDSHAKE_IO_KERNEL, kernel_log))
        {
            close(fd_io);
            terminar_kernel(EXIT_FAILURE);
        }

        LOG_DEBUG(kernel_log, "[SERVIDOR IO] HANDSHAKE_IO_KERNEL recibido de IO (fd=%d)", fd_io);

        char nombre_io[256];
        if (recv(fd_io, nombre_io, sizeof(nombre_io), 0) <= 0)
        {
            LOG_ERROR(kernel_log, "[SERVIDOR IO] Error al recibir nombre de IO");
            close(fd_io);
            terminar_kernel(EXIT_FAILURE);
        }

        io *nueva_io = malloc(sizeof(io));
        nueva_io->fd = fd_io;
        nueva_io->nombre = strdup(nombre_io);
        nueva_io->estado = IO_OCUPADO;
        nueva_io->proceso_actual = NULL;

        LOCK_CON_LOG(mutex_ios);
        list_add(lista_ios, nueva_io);
        UNLOCK_CON_LOG(mutex_ios);

        LOCK_CON_LOG(mutex_conexiones);
        conectado_io = true;
        UNLOCK_CON_LOG(mutex_conexiones);

        LOG_DEBUG(kernel_log, "[SERVIDOR IO] IO '%s' conectada exitosamente (fd=%d)", nueva_io->nombre, fd_io);

        int *arg = malloc(sizeof(int));
        *arg = fd_io;
        pthread_t *hilo = malloc(sizeof(pthread_t));
        if (pthread_create(hilo, NULL, atender_io, arg) != 0)
        {
            LOG_ERROR(kernel_log, "[SERVIDOR IO] Error al crear hilo para atender IO '%s' (fd=%d)", nueva_io->nombre, fd_io);
            LOCK_CON_LOG(mutex_ios);
            list_remove_element(lista_ios, nueva_io);
            UNLOCK_CON_LOG(mutex_ios);
            free(nueva_io->nombre);
            free(nueva_io);
            close(fd_io);
            free(arg);
            free(hilo);
            terminar_kernel(EXIT_FAILURE);
        }
        LOCK_CON_LOG(mutex_hilos);
        list_add(lista_hilos, hilo);
        LOG_DEBUG(kernel_log, "[SERVIDOR IO] Hilo %d agregado para atender IO '%s' (fd=%d)", *hilo, nueva_io->nombre, fd_io);
        UNLOCK_CON_LOG(mutex_hilos);
    }

    terminar_hilo();
    return NULL;
}

void *atender_io(void *arg)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    int fd_io = *(int *)arg;
    free(arg);

    // Encontrar la IO asociada a este file descriptor usando función centralizada

    LOCK_CON_LOG(mutex_ios);

    io *dispositivo_io = buscar_io_por_fd(fd_io);
    UNLOCK_CON_LOG(mutex_ios);

    if (!dispositivo_io)
    {
        LOG_ERROR(kernel_log, "[SERVIDOR IO] No se encontró IO con fd=%d", fd_io);
        close(fd_io);
        terminar_hilo();
        return NULL;
    }

    // Verificar si hay procesos encolados para dicha IO y enviarlo a la misma
    LOG_DEBUG(kernel_log, "[SERVIDOR IO] IO correctamente conectada (fd=%d, nombre='%s'), verificando si hay procesos esperando", fd_io, dispositivo_io->nombre);
    verificar_procesos_bloqueados(dispositivo_io);

    LOG_DEBUG(kernel_log, "[SERVIDOR IO] Atendiendo IO '%s' (fd=%d)", dispositivo_io->nombre, fd_io);

    op_code cop;
    while ((cop = recibir_operacion(fd_io)) != -1)
    {
        switch (cop)
        {
        case IO_FINALIZADA_OP:
        {
            int pid_finalizado;
            if (recv(fd_io, &pid_finalizado, sizeof(int), 0) != sizeof(int))
            {
                LOG_ERROR(kernel_log, "[SERVIDOR IO] Error al recibir PID finalizado de IO '%s'", dispositivo_io->nombre);
                continue;
            }
            t_pcb *pcb_fin = buscar_pcb(pid_finalizado);

            LOG_DEBUG(kernel_log, "[SERVIDOR IO] IO_FINALIZADA_OP, verificando el estado de PCB con PID %d", pid_finalizado);

            LOCK_CON_LOG_PCB(pcb_fin->mutex, pcb_fin->PID);

            if (pcb_fin->Estado == SUSP_BLOCKED)
            {
                log_info(kernel_log, AMARILLO("## (%d) finalizó IO y pasa a SUSP_READY"), pid_finalizado);
                cambiar_estado_pcb(pcb_fin, SUSP_READY);
                UNLOCK_CON_LOG_PCB(pcb_fin->mutex, pcb_fin->PID);

                SEM_POST(sem_procesos_rechazados);
            }
            else if (pcb_fin->Estado == BLOCKED)
            {
                log_info(kernel_log, AMARILLO("## (%d) finalizó IO y pasa a READY"), pid_finalizado);
                cambiar_estado_pcb(pcb_fin, READY);
                UNLOCK_CON_LOG_PCB(pcb_fin->mutex, pcb_fin->PID);
            }
            else
            {
                LOG_ERROR(kernel_log, AZUL("[SERVIDOR IO] PID %d finalizó IO pero ya se encuentra en %s"), pid_finalizado, estado_to_string(pcb_fin->Estado));
                UNLOCK_CON_LOG_PCB(pcb_fin->mutex, pcb_fin->PID);
                terminar_kernel(EXIT_FAILURE);
            }

            // Verificar si hay procesos encolados para dicha IO y enviarlo a la misma
            verificar_procesos_bloqueados(dispositivo_io);
            break;
        }
        default:
            LOG_ERROR(kernel_log, "[SERVIDOR IO] Código op desconocido recibido desde IO '%s' (fd=%d): %d", dispositivo_io->nombre, fd_io, cop);
            terminar_kernel(EXIT_FAILURE);
            break;
        }
    }

    // Evitar que nuevos procesos se envíen a esta IO

    LOCK_CON_LOG(mutex_ios);

    dispositivo_io->estado = IO_OCUPADO;
    UNLOCK_CON_LOG(mutex_ios);

    // IO desconectada
    LOG_DEBUG(kernel_log, "[SERVIDOR IO] IO '%s' desconectada (fd=%d)", dispositivo_io->nombre, fd_io);

    // Mover a EXIT los procesos relacionados
    exit_procesos_relacionados(dispositivo_io);

    // Eliminar la IO de la lista global

    LOCK_CON_LOG(mutex_ios);

    for (int i = 0; i < list_size(lista_ios); i++)
    {
        if (list_get(lista_ios, i) == dispositivo_io)
        {
            list_remove(lista_ios, i);
            break;
        }
    }
    UNLOCK_CON_LOG(mutex_ios);

    // Liberar la estructura IO
    free(dispositivo_io->nombre);
    free(dispositivo_io);
    close(fd_io);

    terminar_hilo();
    return NULL;
}