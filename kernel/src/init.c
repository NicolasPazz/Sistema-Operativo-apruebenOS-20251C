#include "../headers/kernel.h"
#include "../headers/CPUKernel.h"
#include "../headers/syscalls.h"
#include <libgen.h>

/////////////////////////////// Declaracion de variables globales ///////////////////////////////
// Logger
t_log* kernel_log;

// Hashmap de cronometros por PCB
t_dictionary* tiempos_por_pid;
t_dictionary* archivo_por_pcb;

// Sockets
int fd_dispatch;
int fd_cpu_dispatch;
int fd_interrupt;
int fd_cpu_interrupt;
int fd_memoria;
int fd_kernel_io;
int fd_io;

// Config
t_config* kernel_config;
char* IP_MEMORIA;
char* PUERTO_MEMORIA;
char* PUERTO_ESCUCHA_DISPATCH;
char* PUERTO_ESCUCHA_INTERRUPT;
char* PUERTO_ESCUCHA_IO;
char* ALGORITMO_CORTO_PLAZO;
char* ALGORITMO_INGRESO_A_READY;
double ALFA;
char* TIEMPO_SUSPENSION;
double ESTIMACION_INICIAL;
char* LOG_LEVEL;

// Colas de Estados
t_list* cola_new;
t_list* cola_ready;
t_list* cola_running;
t_list* cola_blocked;
t_list* cola_susp_ready;
t_list* cola_susp_blocked;
t_list* cola_exit;
t_list* cola_procesos; // Cola con TODOS los procesos sin importar el estado (Procesos totales del sistema)
t_list* cola_rechazados; // Cola de procesos rechazados por falta de memoria
t_list* pcbs_bloqueados_por_dump_memory;
t_list* pcbs_esperando_io;
t_queue* cola_interrupciones;

// Listas y semaforos de CPUs y IOs conectadas
t_list* lista_cpus;
pthread_mutex_t mutex_lista_cpus;
t_list* lista_ios;
pthread_mutex_t mutex_ios;

// Conexiones minimas
bool conectado_cpu = false;
bool conectado_io = false;
bool conectado_memoria = false;
pthread_mutex_t mutex_conexiones;

// Semaforos de planificacion
pthread_mutex_t mutex_cola_new;
pthread_mutex_t mutex_cola_susp_ready;
pthread_mutex_t mutex_cola_susp_blocked;
pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_running;
pthread_mutex_t mutex_cola_blocked;
pthread_mutex_t mutex_cola_exit;
pthread_mutex_t mutex_cola_procesos;
pthread_mutex_t mutex_cola_rechazados;
pthread_mutex_t mutex_pcbs_esperando_io;
pthread_mutex_t mutex_cola_interrupciones;
sem_t sem_proceso_a_new;
sem_t sem_proceso_a_susp_ready;
sem_t sem_proceso_a_susp_blocked;
sem_t sem_proceso_a_ready;
sem_t sem_proceso_a_running;
sem_t sem_proceso_a_blocked;
sem_t sem_proceso_a_exit;
sem_t sem_proceso_a_rechazados;
sem_t sem_susp_ready_vacia;
sem_t sem_finalizacion_de_proceso;
sem_t sem_cpu_disponible;
sem_t sem_replanificar_srt;
sem_t sem_interrupciones;

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                       INICIALIZACIONES                                       //
//////////////////////////////////////////////////////////////////////////////////////////////////

void iniciar_config_kernel() {
    kernel_config = iniciar_config("kernel/kernel.config");

    IP_MEMORIA = config_get_string_value(kernel_config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_string_value(kernel_config, "PUERTO_MEMORIA");
    PUERTO_ESCUCHA_DISPATCH = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_DISPATCH");
    PUERTO_ESCUCHA_INTERRUPT = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_INTERRUPT");
    PUERTO_ESCUCHA_IO = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_IO");
    ALGORITMO_CORTO_PLAZO = config_get_string_value(kernel_config, "ALGORITMO_CORTO_PLAZO");
    ALGORITMO_INGRESO_A_READY = config_get_string_value(kernel_config, "ALGORITMO_INGRESO_A_READY");
    ALFA = config_get_double_value(kernel_config, "ALFA");
    ESTIMACION_INICIAL = config_get_double_value(kernel_config, "ESTIMACION_INICIAL");
    TIEMPO_SUSPENSION = config_get_string_value(kernel_config, "TIEMPO_SUSPENSION");
    LOG_LEVEL = config_get_string_value(kernel_config, "LOG_LEVEL");

    if (!IP_MEMORIA || !PUERTO_MEMORIA || !PUERTO_ESCUCHA_DISPATCH || !PUERTO_ESCUCHA_INTERRUPT ||
        !PUERTO_ESCUCHA_IO || !ALGORITMO_CORTO_PLAZO || !ALGORITMO_INGRESO_A_READY ||
        !ALFA || !ESTIMACION_INICIAL || !TIEMPO_SUSPENSION || !LOG_LEVEL) {
        printf("iniciar_config_kernel: Faltan campos obligatorios en kernel.config\n");
        exit(EXIT_FAILURE);
    } else {
        /*
        printf("IP_MEMORIA: %s", IP_MEMORIA);
        printf("PUERTO_MEMORIA: %s", PUERTO_MEMORIA);
        printf("PUERTO_ESCUCHA_DISPATCH: %s", PUERTO_ESCUCHA_DISPATCH);
        printf("PUERTO_ESCUCHA_INTERRUPT: %s", PUERTO_ESCUCHA_INTERRUPT);
        printf("PUERTO_ESCUCHA_IO: %s", PUERTO_ESCUCHA_IO);
        printf("ALGORITMO_CORTO_PLAZO: %s", ALGORITMO_CORTO_PLAZO);
        printf("ALGORITMO_INGRESO_A_READY: %s", ALGORITMO_INGRESO_A_READY);
        printf("ALFA: %.2f", ALFA);
        printf("ESTIMACION_INICIAL: %.2f", ESTIMACION_INICIAL);
        printf("TIEMPO_SUSPENSION: %s", TIEMPO_SUSPENSION);
        printf("LOG_LEVEL: %s", LOG_LEVEL);*/
    }
}

void iniciar_logger_kernel() {
    kernel_log = iniciar_logger("kernel/kernel.log", "KERNEL", 1, log_level_from_string(LOG_LEVEL));
    log_trace(kernel_log, "Kernel log iniciado correctamente!");
}

void iniciar_estados_kernel() { 
    cola_new = list_create();
    cola_ready = list_create();
    cola_running = list_create();
    cola_blocked = list_create();
    cola_susp_ready = list_create();
    cola_susp_blocked = list_create();
    cola_exit = list_create();
    cola_procesos = list_create();
    cola_rechazados = list_create();
    pcbs_bloqueados_por_dump_memory = list_create();
    pcbs_esperando_io = list_create();
}

void iniciar_sincronizacion_kernel() {
    pthread_mutex_init(&mutex_lista_cpus, NULL);
    pthread_mutex_init(&mutex_ios, NULL);
    pthread_mutex_init(&mutex_conexiones, NULL);

    pthread_mutex_init(&mutex_cola_new, NULL);
    pthread_mutex_init(&mutex_cola_susp_ready, NULL);
    pthread_mutex_init(&mutex_cola_susp_blocked, NULL);
    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_running, NULL);
    pthread_mutex_init(&mutex_cola_blocked, NULL);
    pthread_mutex_init(&mutex_cola_exit, NULL);
    pthread_mutex_init(&mutex_cola_procesos, NULL);
    pthread_mutex_init(&mutex_cola_rechazados, NULL);
    pthread_mutex_init(&mutex_pcbs_esperando_io, NULL);
    pthread_mutex_init(&mutex_cola_interrupciones, NULL);

    sem_init(&sem_proceso_a_new, 0, 0);
    sem_init(&sem_proceso_a_susp_ready, 0, 0);
    sem_init(&sem_proceso_a_susp_blocked, 0, 0);
    sem_init(&sem_proceso_a_ready, 0, 0);
    sem_init(&sem_proceso_a_running, 0, 0);
    sem_init(&sem_proceso_a_blocked, 0, 0);
    sem_init(&sem_proceso_a_exit, 0, 0);
    sem_init(&sem_proceso_a_rechazados, 0, 0);
    sem_init(&sem_susp_ready_vacia, 0, 1);
    sem_init(&sem_finalizacion_de_proceso, 0, 0);
    sem_init(&sem_cpu_disponible, 0, 0);
    sem_init(&sem_replanificar_srt, 0, 0);
    sem_init(&sem_interrupciones, 0, 0);

    lista_cpus = list_create();
    lista_ios = list_create();
    cola_interrupciones = queue_create();

    conectado_cpu = false;
    conectado_io = false;
    conectado_memoria = false;
}

void iniciar_diccionario_tiempos() {
    tiempos_por_pid = dictionary_create();
}

void iniciar_diccionario_archivos_por_pcb() {
    archivo_por_pcb = dictionary_create();
}

void terminar_kernel() {
    log_destroy(kernel_log);
    config_destroy(kernel_config);

    list_destroy(cola_new);
    list_destroy(cola_ready);
    list_destroy(cola_running);
    list_destroy(cola_blocked);
    list_destroy(cola_susp_ready);
    list_destroy(cola_susp_blocked);
    list_destroy(cola_exit);
    list_destroy(cola_procesos);
    list_destroy(cola_rechazados);
    list_destroy(pcbs_bloqueados_por_dump_memory);
    list_destroy(pcbs_esperando_io);

    pthread_mutex_destroy(&mutex_lista_cpus);
    pthread_mutex_destroy(&mutex_ios);
    pthread_mutex_destroy(&mutex_conexiones);

    pthread_mutex_destroy(&mutex_cola_new);
    pthread_mutex_destroy(&mutex_cola_susp_ready);
    pthread_mutex_destroy(&mutex_cola_susp_blocked);
    pthread_mutex_destroy(&mutex_cola_ready);
    pthread_mutex_destroy(&mutex_cola_running);
    pthread_mutex_destroy(&mutex_cola_blocked);
    pthread_mutex_destroy(&mutex_cola_exit);
    pthread_mutex_destroy(&mutex_cola_procesos);
    pthread_mutex_destroy(&mutex_cola_rechazados);
    pthread_mutex_destroy(&mutex_pcbs_esperando_io);
    pthread_mutex_destroy(&mutex_cola_interrupciones);

    sem_destroy(&sem_proceso_a_new);
    sem_destroy(&sem_proceso_a_susp_ready);
    sem_destroy(&sem_proceso_a_susp_blocked);
    sem_destroy(&sem_proceso_a_ready);
    sem_destroy(&sem_proceso_a_running);
    sem_destroy(&sem_proceso_a_blocked);
    sem_destroy(&sem_proceso_a_exit);
    sem_destroy(&sem_proceso_a_rechazados);
    sem_destroy(&sem_susp_ready_vacia);
    sem_destroy(&sem_finalizacion_de_proceso);
    sem_destroy(&sem_cpu_disponible);
    sem_destroy(&sem_replanificar_srt);
    sem_destroy(&sem_interrupciones);

    list_destroy(lista_cpus);
    list_destroy(lista_ios);
    queue_destroy(cola_interrupciones);

    close(fd_cpu_dispatch);
    close(fd_cpu_interrupt);
    close(fd_memoria);
    close(fd_kernel_io);
    close(fd_io);
    close(fd_dispatch);
    close(fd_interrupt);
}

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                            MEMORIA                                           //
//////////////////////////////////////////////////////////////////////////////////////////////////

void* hilo_cliente_memoria(void* _) {
    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, kernel_log);

    if (fd_memoria == -1) {
        log_error(kernel_log, "Error al conectar Kernel a Memoria");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    int handshake = HANDSHAKE_MEMORIA_KERNEL;
    if (send(fd_memoria, &handshake, sizeof(int), 0) <= 0) {
        log_error(kernel_log, "Error al enviar handshake a Memoria");
        close(fd_memoria);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&mutex_conexiones);
    conectado_memoria = true;
    pthread_mutex_unlock(&mutex_conexiones);

    log_trace(kernel_log, "HANDSHAKE_MEMORIA_KERNEL: Kernel conectado correctamente a Memoria (fd=%d)", fd_memoria);

    return NULL;
}

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                         CPU DISPATCH                                         //
//////////////////////////////////////////////////////////////////////////////////////////////////

void* hilo_servidor_dispatch(void* _) {
    fd_dispatch = iniciar_servidor(PUERTO_ESCUCHA_DISPATCH, kernel_log, "Kernel Dispatch");

    while (1) {
        int fd_cpu_dispatch = esperar_cliente(fd_dispatch, kernel_log);
        if (fd_cpu_dispatch == -1) {
            log_error(kernel_log, "hilo_servidor_dispatch: Error al recibir cliente");
            continue;
        }

        if (!validar_handshake(fd_cpu_dispatch, HANDSHAKE_CPU_KERNEL_DISPATCH, kernel_log)) {
            close(fd_cpu_dispatch);
            continue;
        }

        int id_cpu;
        if (recv(fd_cpu_dispatch, &id_cpu, sizeof(int), 0) <= 0) {
            log_error(kernel_log, "Error al recibir ID de CPU desde Dispatch");
            close(fd_cpu_dispatch);
            continue;
        }

        cpu* nueva_cpu = malloc(sizeof(cpu));
        nueva_cpu->fd = fd_cpu_dispatch;
        nueva_cpu->id = id_cpu;
        nueva_cpu->tipo_conexion = CPU_DISPATCH;
        nueva_cpu->pid = -1;
        nueva_cpu->instruccion_actual = -1;

        log_debug(kernel_log, "hilo_servidor_dispatch: esperando mutex_lista_cpus para agregar nueva CPU (fd=%d, ID=%d)", fd_cpu_dispatch, id_cpu);
        pthread_mutex_lock(&mutex_lista_cpus);
        log_debug(kernel_log, "hilo_servidor_dispatch: bloqueando mutex_lista_cpus para agregar nueva CPU (fd=%d, ID=%d)", fd_cpu_dispatch, id_cpu);
        list_add(lista_cpus, nueva_cpu);
        pthread_mutex_unlock(&mutex_lista_cpus);

        sem_post(&sem_cpu_disponible);
        solicitar_replanificacion_srt();
        log_trace(kernel_log, "hilo_servidor_dispatch: replanificacion solicitada");
        log_debug(kernel_log, "hilo_servidor_dispatch: Semaforo CPU DISPONIBLE aumentado por nueva CPU (fd=%d, ID=%d)", fd_cpu_dispatch, id_cpu);

        pthread_mutex_lock(&mutex_conexiones);
        conectado_cpu = true;
        pthread_mutex_unlock(&mutex_conexiones);

        log_trace(kernel_log, "HANDSHAKE_CPU_KERNEL_DISPATCH: CPU conectada exitosamente a Dispatch (fd=%d), ID=%d", nueva_cpu->fd, nueva_cpu->id);

        int* arg = malloc(sizeof(int));
        *arg = fd_cpu_dispatch;
        pthread_t hilo;
        pthread_create(&hilo, NULL, atender_cpu_dispatch, arg);
        pthread_detach(hilo);
    }

    return NULL;
}

void* atender_cpu_dispatch(void* arg) {
    int fd_cpu_dispatch = *(int*)arg;
    free(arg);

    op_code cop;
    while ((cop = recibir_operacion(fd_cpu_dispatch)) != -1) {

        cpu* cpu_actual = get_cpu_from_fd(fd_cpu_dispatch);

        if (!cpu_actual) {
            log_error(kernel_log, "atender_cpu_dispatch: No se encontró CPU asociada al fd=%d", fd_cpu_dispatch);
            close(fd_cpu_dispatch);
            return NULL;
        }
        
        pthread_mutex_lock(&mutex_lista_cpus);
        cpu_actual->instruccion_actual = cop;
        int pid = cpu_actual->pid;
        pthread_mutex_unlock(&mutex_lista_cpus);

        log_trace(kernel_log, "atender_cpu_dispatch: CPU ID=%d está procesando operación %d para pid %d", cpu_actual->id, cop, pid);

        switch (cop) {
            case INIT_PROC_OP: {
                log_info(kernel_log, VERDE("## (PID: %d) Solicitó syscall: ")ROJO("INIT_PROC"), pid);
                log_trace(kernel_log, "INIT_PROC_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);
            
                // ========== RECIBIR PARÁMETROS ==========
                t_list* parametros_init_proc = recibir_contenido_paquete(fd_cpu_dispatch);
                if (!parametros_init_proc) {
                    log_error(kernel_log, "Error al recibir parámetros para INIT_PROC_OP desde CPU Dispatch");
                    break;
                }

                // Nombre y size
                char* nombre = (char*)list_get(parametros_init_proc, 0);
                int size = *(int*)list_get(parametros_init_proc, 1);
                log_trace(kernel_log, "Proceso: %s Size: %d recibido de CPU Dispatch (fd=%d)", nombre, size, fd_cpu_dispatch);   
                log_debug(kernel_log, "atender_cpu_dispatch: Iniciando nuevo proceso con nombre de archivo '%s' y size %d", nombre, size);
                
                // ========== EJECUTAR SYSCALL ==========
                INIT_PROC(nombre, size);

                // Liberar memoria
                list_destroy_and_destroy_elements(parametros_init_proc, free);
                break;
            }

            case IO_OP: {
                log_info(kernel_log, VERDE("## (PID: %d) Solicitó syscall: ")ROJO("IO"), pid);
                log_trace(kernel_log, "IO_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);

                // ========== RECIBIR PARÁMETROS ==========
                t_list* parametros_io = recibir_contenido_paquete(fd_cpu_dispatch);
                if (!parametros_io) {
                    log_error(kernel_log, "Error al recibir parámetros para IO_OP desde CPU Dispatch");
                    break;
                }

                // Nombre IO, Tiempo a usar y PC
                char* nombre_IO = (char*)list_get(parametros_io, 0);
                int cant_tiempo = *(int*)list_get(parametros_io, 1);
                int PC = *(int*)list_get(parametros_io, 2);
            
                log_trace(kernel_log, "Recibida de CPU Dispatch IO '%s' con tiempo=%d", nombre_IO, cant_tiempo);

                // Buscar Proceso PCB para mandarlo a IO y bloquearlo
                t_pcb* pcb_a_io = buscar_pcb(pid);
                if (!pcb_a_io) {
                    log_error(kernel_log, "No se encontró PCB para PID=%d", pid);
                    list_destroy_and_destroy_elements(parametros_io, free);
                    break;
                }
                // Actualizar el PC con el que me dijo CPU
                pcb_a_io->PC = PC;

                pthread_mutex_lock(&mutex_lista_cpus);
                cpu_actual->pid = -1;
                cpu_actual->instruccion_actual = -1;
                pthread_mutex_unlock(&mutex_lista_cpus);

                sem_post(&sem_cpu_disponible);
                solicitar_replanificacion_srt();
                log_trace(kernel_log, "atender_cpu_dispatch_IO: replanificacion solicitada");

                log_debug(kernel_log, "atender_cpu_dispatch: Semaforo CPU DISPONIBLE aumentado por IO");
                log_trace(kernel_log, "Procesando solicitud de IO '%s' por %d ms para PID=%d", nombre_IO, cant_tiempo, pcb_a_io->PID);
    
                // ========== EJECUTAR SYSCALL ==========
                IO(nombre_IO, cant_tiempo, pcb_a_io);

                list_destroy_and_destroy_elements(parametros_io,free);
                break;
            }            

            case EXIT_OP:
                log_info(kernel_log, VERDE("## (PID: %d) Solicitó syscall: ")ROJO("EXIT"), pid);
                log_trace(kernel_log, "EXIT_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);

                // Obtener PID del proceso que está ejecutando esta CPU
                pthread_mutex_lock(&mutex_lista_cpus);
                int pid_exit = cpu_actual->pid;
                pthread_mutex_unlock(&mutex_lista_cpus);
                
                log_trace(kernel_log, "EXIT_OP asociado a PID=%d", pid);

                // Buscar el PCB en EXEC sin removerlo
                log_debug(kernel_log, "atender_cpu_dispatch: esperando mutex_cola_running para buscar PCB con PID=%d", pid_exit);

                pthread_mutex_lock(&mutex_cola_running);
                log_debug(kernel_log, "atender_cpu_dispatch: bloqueando mutex_cola_running para buscar PCB con PID=%d", pid_exit);
                t_pcb* pcb_a_finalizar = NULL;
                for (int i = 0; i < list_size(cola_running); i++) {
                    t_pcb* pcb = list_get(cola_running, i);
                    if (pcb && pcb->PID == pid_exit) {
                        pcb_a_finalizar = pcb;
                        break;
                    }
                }
                pthread_mutex_unlock(&mutex_cola_running);

                // Confirmar que se encontró el PCB
                if (pcb_a_finalizar) {
                    // Cambiar estado y finalizar
                    cambiar_estado_pcb(pcb_a_finalizar, EXIT_ESTADO);
                    // Llamar a la syscall EXIT para liberar recursos y comunicarse con Memoria
                    //EXIT(pcb_a_finalizar);
                } else {
                    log_error(kernel_log, "EXIT: No se encontró PCB para PID=%d en RUNNING", pid_exit);
                    terminar_kernel();
                    exit(EXIT_FAILURE);
                }

                // Limpiar PID de la CPU asociada
                pthread_mutex_lock(&mutex_lista_cpus);
                cpu_actual->pid = -1; // Limpiar PID de la CPU
                cpu_actual->instruccion_actual = -1; // Limpiar instrucción actual
                pthread_mutex_unlock(&mutex_lista_cpus);
                
                // Liberar CPU para que el planificador pueda usarla
                sem_post(&sem_cpu_disponible);
                solicitar_replanificacion_srt();
                log_trace(kernel_log, "atender_cpu_dispatch_EXIT: replanificacion solicitada");

                log_debug(kernel_log, "EXIT: Semáforo CPU DISPONIBLE aumentado por CPU liberada");
                
                break;

            case DUMP_MEMORY_OP:
                log_info(kernel_log, VERDE("## (PID: %d) Solicitó syscall: ")ROJO("DUMP_MEMORY"), pid);
                log_trace(kernel_log, "DUMP_MEMORY_OP recibido de CPU Dispatch (fd=%d)", fd_cpu_dispatch);
                
                // ========== RECIBIR PARÁMETROS ==========
                t_list* parametros_dump = recibir_contenido_paquete(fd_cpu_dispatch);
                if (!parametros_dump || list_size(parametros_dump) < 2) {
                    log_error(kernel_log, "DUMP_MEMORY_OP: Error al recibir parámetros desde CPU Dispatch. Esperados: 2, recibidos: %d", 
                              parametros_dump ? list_size(parametros_dump) : 0);
                    if (parametros_dump) list_destroy_and_destroy_elements(parametros_dump, free);
                    break;
                }

                int PID = *(int*)list_get(parametros_dump, 0);
                int PC_actualizado = *(int*)list_get(parametros_dump, 1);  // ✅ RECIBIR PC ACTUALIZADO

                // Obtener el PCB del proceso
                t_pcb* pcb_dump = buscar_pcb(PID);
                if (!pcb_dump) {
                    log_error(kernel_log, "DUMP_MEMORY_OP: No se encontró PCB para PID %d", PID);
                    list_destroy_and_destroy_elements(parametros_dump, free);
                    break;
                }

                // ✅ ACTUALIZAR PC DEL PCB (igual que en IO_OP)
                pcb_dump->PC = PC_actualizado;
                log_trace(kernel_log, "DUMP_MEMORY: PC actualizado para PID %d: %d", PID, PC_actualizado);

                // ========== LIBERAR CPU (IGUAL QUE EXIT E IO) ==========
                // Limpiar PID de la CPU asociada
                pthread_mutex_lock(&mutex_lista_cpus);
                cpu_actual->pid = -1; // Limpiar PID de la CPU
                cpu_actual->instruccion_actual = -1; // Limpiar instrucción actual
                pthread_mutex_unlock(&mutex_lista_cpus);
                
                // Liberar CPU para que el planificador pueda usarla
                sem_post(&sem_cpu_disponible);
                solicitar_replanificacion_srt();
                log_trace(kernel_log, "atender_cpu_dispatch_DUMP: replanificacion solicitada");

                log_debug(kernel_log, "DUMP_MEMORY: Semáforo CPU DISPONIBLE aumentado por CPU liberada");

                // ========== EJECUTAR SYSCALL ==========
                DUMP_MEMORY(pcb_dump);

                list_destroy_and_destroy_elements(parametros_dump, free);
                break;
                
            case SHUTDOWN_OP:
                log_info(kernel_log, "SHUTDOWN_OP recibido - Finalizando Kernel");
                terminar_kernel();
                exit(EXIT_SUCCESS);
                break;
                
            default:
                log_error(kernel_log, "(PID: %d) Código op desconocido recibido de Dispatch fd %d: %d", pid, fd_cpu_dispatch, cop);
                break;
        }

        // Limpiar la instrucción actual de la CPU
        pthread_mutex_lock(&mutex_lista_cpus);
        cpu_actual->instruccion_actual = -1; // Valor inválido para indicar que está libre
        pthread_mutex_unlock(&mutex_lista_cpus);
    }

    log_warning(kernel_log, "CPU Dispatch desconectada (fd=%d)", fd_cpu_dispatch);
    
    // Eliminar esta CPU de la lista de CPUs usando función centralizada
    cpu* cpu_eliminada = buscar_y_remover_cpu_por_fd(fd_cpu_dispatch);
    
    if (cpu_eliminada) {
        free(cpu_eliminada);
    }

    close(fd_cpu_dispatch);
    return NULL;
}

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                         CPU INTERRUPT                                        //
//////////////////////////////////////////////////////////////////////////////////////////////////

void* hilo_servidor_interrupt(void* _) {
    fd_interrupt = iniciar_servidor(PUERTO_ESCUCHA_INTERRUPT, kernel_log, "Kernel Interrupt");

    while (1) {
        int fd_cpu_interrupt = esperar_cliente(fd_interrupt, kernel_log);
        if (fd_cpu_interrupt == -1) {
            log_error(kernel_log, "hilo_servidor_interrupt: Error al recibir cliente");
            continue;
        }

        if (!validar_handshake(fd_cpu_interrupt, HANDSHAKE_CPU_KERNEL_INTERRUPT, kernel_log)) {
            close(fd_cpu_interrupt);
            continue;
        }

        int id_cpu;
        if (recv(fd_cpu_interrupt, &id_cpu, sizeof(int), 0) <= 0) {
            log_error(kernel_log, "Error al recibir ID de CPU desde Interrupt");
            close(fd_cpu_interrupt);
            continue;
        }

        cpu* nueva_cpu = malloc(sizeof(cpu));
        nueva_cpu->fd = fd_cpu_interrupt;
        nueva_cpu->id = id_cpu;
        nueva_cpu->tipo_conexion = CPU_INTERRUPT;
        nueva_cpu->pid = -1;
        nueva_cpu->instruccion_actual = -1;
        
        pthread_mutex_lock(&mutex_lista_cpus);
        list_add(lista_cpus, nueva_cpu);
        pthread_mutex_unlock(&mutex_lista_cpus);

        pthread_mutex_lock(&mutex_conexiones);
        conectado_cpu = true;
        pthread_mutex_unlock(&mutex_conexiones);

        log_trace(kernel_log, "HANDSHAKE_CPU_KERNEL_INTERRUPT: CPU conectada exitosamente a Interrupt (fd=%d), ID=%d", nueva_cpu->fd, nueva_cpu->id);

        int* arg = malloc(sizeof(int));
        *arg = fd_cpu_interrupt;
        pthread_t hilo;
        pthread_create(&hilo, NULL, atender_cpu_interrupt, arg);
        pthread_detach(hilo);
    }

    return NULL;
}

void* atender_cpu_interrupt(void* arg) {
    /*int fd_cpu_interrupt = *(int*)arg;
    free(arg);

    op_code cop;
    while (recv(fd_cpu_interrupt, &cop, sizeof(op_code), 0) > 0) {
        switch (cop) {
            default:
                log_error(kernel_log, "Codigo op desconocido recibido de Interrupt (fd=%d): %d", fd_cpu_interrupt, cop);
                terminar_kernel();
                exit(EXIT_FAILURE);
        }
    }

    log_warning(kernel_log, "CPU Interrupt desconectada (fd=%d)", fd_cpu_interrupt);
    close(fd_cpu_interrupt);*/
    return NULL;
}

  //////////////////////////////////////////////////////////////////////////////////////////////////
 //                                              IO                                              //
//////////////////////////////////////////////////////////////////////////////////////////////////

void* hilo_servidor_io(void* _) {
    fd_kernel_io = iniciar_servidor(PUERTO_ESCUCHA_IO, kernel_log, "Kernel IO");

    while (1) {
        int fd_io = esperar_cliente(fd_kernel_io, kernel_log);
        if (fd_io == -1) {
            log_error(kernel_log, "hilo_servidor_io: Error al recibir cliente");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        if (!validar_handshake(fd_io, HANDSHAKE_IO_KERNEL, kernel_log)) {
            close(fd_io);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        log_debug(kernel_log, "hilo_servidor_io: HANDSHAKE_IO_KERNEL recibido de IO (fd=%d)", fd_io);

        char nombre_io[256];
        if (recv(fd_io, nombre_io, sizeof(nombre_io), 0) <= 0) {
            log_error(kernel_log, "hilo_servidor_io: Error al recibir nombre de IO");
            close(fd_io);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        io* nueva_io = malloc(sizeof(io));
        nueva_io->fd = fd_io;
        nueva_io->nombre = strdup(nombre_io); 
        nueva_io->estado = IO_OCUPADO;
        nueva_io->proceso_actual = NULL;

        log_debug(kernel_log, "hilo_servidor_io: esperando mutex_ios para agregar nueva IO (fd=%d, nombre='%s')", fd_io, nueva_io->nombre);
        pthread_mutex_lock(&mutex_ios);
        log_debug(kernel_log, "hilo_servidor_io: bloqueando mutex_ios para agregar nueva IO (fd=%d, nombre='%s')", fd_io, nueva_io->nombre);
        list_add(lista_ios, nueva_io);
        pthread_mutex_unlock(&mutex_ios);

        log_debug(kernel_log, "hilo_servidor_io: esperando mutex_conexiones para verificar procesos bloqueados en IO '%s'", nueva_io->nombre);
        pthread_mutex_lock(&mutex_conexiones);
        log_debug(kernel_log, "hilo_servidor_io: bloqueando mutex_conexiones para verificar procesos bloqueados en IO '%s'", nueva_io->nombre);
        conectado_io = true;
        pthread_mutex_unlock(&mutex_conexiones);

        log_trace(kernel_log, "hilo_servidor_io: HANDSHAKE_IO_KERNEL: IO '%s' conectada exitosamente (fd=%d)", nueva_io->nombre, fd_io);

        int* arg = malloc(sizeof(int));
        *arg = fd_io;
        pthread_t hilo;
        pthread_create(&hilo, NULL, atender_io, arg);
        pthread_detach(hilo);
    }

    return NULL;
}

void* atender_io(void* arg) {
    int fd_io = *(int*)arg;
    free(arg);

    // Encontrar la IO asociada a este file descriptor usando función centralizada
    pthread_mutex_lock(&mutex_ios);
    io* dispositivo_io = buscar_io_por_fd(fd_io);
    pthread_mutex_unlock(&mutex_ios);

    if (!dispositivo_io) {
        log_error(kernel_log, "atender_io: No se encontró IO con fd=%d", fd_io);
        close(fd_io);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Verificar si hay procesos encolados para dicha IO y enviarlo a la misma
    log_debug(kernel_log, "IO correctamente conectada (fd=%d, nombre='%s'), verificando si hay procesos esperando", fd_io, dispositivo_io->nombre);
    verificar_procesos_bloqueados(dispositivo_io);

    log_trace(kernel_log, "Atendiendo IO '%s' (fd=%d)", dispositivo_io->nombre, fd_io);

    op_code cop;
    while (recv(fd_io, &cop, sizeof(op_code), 0) > 0) {
        switch (cop) {
            case IO_FINALIZADA_OP: {
                int pid_finalizado;
                if (recv(fd_io, &pid_finalizado, sizeof(int), 0) != sizeof(int)) {
                    log_error(kernel_log, "Error al recibir PID finalizado de IO '%s'", dispositivo_io->nombre);
                    continue;
                }
                
                // Buscar PCB para cambiar estado dependiendo de si estaba en SUSP_BLOCKED o BLOCKED
                t_pcb* pcb = buscar_pcb(pid_finalizado);
                if (!pcb) {
                    log_error(kernel_log, "No se encontró PCB para PID=%d al finalizar IO", pid_finalizado);
                    continue;
                }

                if (pcb->Estado == SUSP_BLOCKED) {
                    cambiar_estado_pcb(pcb, SUSP_READY);
                    log_info(kernel_log, "## (PID: %d) finalizó IO y pasa a SUSP_READY", pid_finalizado);

                } else if (pcb->Estado == BLOCKED) {
                    cambiar_estado_pcb(pcb, READY);
                    log_info(kernel_log, "## (PID: %d) finalizó IO y pasa a READY", pid_finalizado);
                    
                } else {
                    log_warning(kernel_log, "PID=%d finalizó IO pero estaba en estado %s", pid_finalizado, estado_to_string(pcb->Estado));
                }

                // Verificar si hay procesos encolados para dicha IO y enviarlo a la misma
                verificar_procesos_bloqueados(dispositivo_io);
                break;
            }
            default:
                log_error(kernel_log, "Código op desconocido recibido desde IO '%s' (fd=%d): %d",
                          dispositivo_io->nombre, fd_io, cop);
                terminar_kernel();
                exit(EXIT_FAILURE);
                break;
        }
    }

    // Evitar que nuevos procesos se envíen a esta IO
    pthread_mutex_lock(&mutex_ios);
    dispositivo_io->estado = IO_OCUPADO;
    pthread_mutex_unlock(&mutex_ios);

    // IO desconectada
    log_warning(kernel_log, "IO '%s' desconectada (fd=%d)", dispositivo_io->nombre, fd_io);

    // Mover a EXIT los procesos relacionados
    exit_procesos_relacionados(dispositivo_io);

    // Eliminar la IO de la lista global
    pthread_mutex_lock(&mutex_ios);
    for (int i = 0; i < list_size(lista_ios); i++) {
        if (list_get(lista_ios, i) == dispositivo_io) {
            list_remove(lista_ios, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_ios);

    // Liberar la estructura IO
    free(dispositivo_io->nombre);
    free(dispositivo_io);
    close(fd_io);

    return NULL;
}

void verificar_procesos_bloqueados(io* io) {
    if (!io) {
        log_error(kernel_log, "verificar_procesos_bloqueados: IO no válida");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_debug(kernel_log, "verificar_procesos_bloqueados: esperando mutex_pcbs_esperando_io para IO '%s'", io->nombre);
    pthread_mutex_lock(&mutex_pcbs_esperando_io);
    log_debug(kernel_log, "verificar_procesos_bloqueados: bloqueando mutex_pcbs_esperando_io para IO '%s'", io->nombre);

    // Obtener el primer proceso esperando una IO con ese nombre
    t_pcb_io* pendiente = obtener_pcb_esperando_io(io->nombre);

    if (!pendiente) {
        log_trace(kernel_log, "No hay procesos pendientes para la IO '%s'", io->nombre);
        pthread_mutex_lock(&mutex_ios);
        io->estado = IO_DISPONIBLE;
        io->proceso_actual = NULL;
        pthread_mutex_unlock(&mutex_ios);
        pthread_mutex_unlock(&mutex_pcbs_esperando_io);
        return;
    }

    log_trace(kernel_log, "Asignando IO '%s' (fd=%d) al proceso PID=%d por %d ms", io->nombre, io->fd, pendiente->pcb->PID, pendiente->tiempo_a_usar);

    asignar_proceso(io, pendiente);

    pthread_mutex_unlock(&mutex_pcbs_esperando_io);
}

t_pcb_io* obtener_pcb_esperando_io(char* nombre_io) {
    // Obtener primer proceso esperando una IO con el nombre dado
    for (int i = 0; i < list_size(pcbs_esperando_io); i++) {
        t_pcb_io* pcb_io = list_get(pcbs_esperando_io, i);
        if (pcb_io->io && strcmp(pcb_io->io->nombre, nombre_io) == 0) {
            return list_remove(pcbs_esperando_io, i);
        }
    }
    return NULL;
}

void asignar_proceso(io* dispositivo, t_pcb_io* proceso) {
    dispositivo->estado = IO_OCUPADO;
    dispositivo->proceso_actual = proceso->pcb;
    proceso->io = dispositivo;

    // Crear paquete serializado
    t_paquete* paquete = crear_paquete_op(IO_OP);
    agregar_a_paquete(paquete, dispositivo->nombre, strlen(dispositivo->nombre) + 1); // nombre de la IO
    agregar_entero_a_paquete(paquete, proceso->tiempo_a_usar); // tiempo
    agregar_entero_a_paquete(paquete, proceso->pcb->PID); // pid

    enviar_paquete(paquete, dispositivo->fd);
    eliminar_paquete(paquete);

    log_trace(kernel_log, "Enviado PID=%d a IO '%s' por %d ms", proceso->pcb->PID, dispositivo->nombre, proceso->tiempo_a_usar);
}

void exit_procesos_relacionados(io* dispositivo) {
    // Exit proceso usando esta instancia de io
    if (dispositivo->proceso_actual != NULL) {
        log_warning(kernel_log, "Proceso PID=%d ejecutando en IO desconectada, moviendo a EXIT", dispositivo->proceso_actual->PID);
        cambiar_estado_pcb(dispositivo->proceso_actual, EXIT_ESTADO);
        dispositivo->proceso_actual = NULL;
    } else {
        log_warning(kernel_log, "IO '%s' se desconectó sin proceso en ejecución", dispositivo->nombre);
    }
    
    if (list_is_empty(pcbs_esperando_io)) return;

    // Verificar si hay otros dispositivos IO con el mismo nombre
    bool hay_otras_io_disponibles = false;
    io* otra_io = NULL;

    pthread_mutex_lock(&mutex_ios);
    for (int j = 0; j < list_size(lista_ios); j++) {
        otra_io = list_get(lista_ios, j);
        if (otra_io != dispositivo && strcmp(otra_io->nombre, dispositivo->nombre) == 0) {
            hay_otras_io_disponibles = true;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_ios);

    // Si no hay otras IO con el mismo nombre, mover los procesos afectados a EXIT
    if (!hay_otras_io_disponibles) {
        t_list* pcbs_afectados = list_create();

        pthread_mutex_lock(&mutex_pcbs_esperando_io);
        int i = 0;
        while (i < list_size(pcbs_esperando_io)) {
            t_pcb_io* pcb_io = list_get(pcbs_esperando_io, i);
            if (strcmp(pcb_io->io->nombre, dispositivo->nombre) == 0) {
                list_add(pcbs_afectados, pcb_io);
                list_remove(pcbs_esperando_io, i);
            } else {
                i++;
            }
        }
        pthread_mutex_unlock(&mutex_pcbs_esperando_io);

        for (int i = 0; i < list_size(pcbs_afectados); i++) {
            t_pcb_io* pcb_io_actual = list_get(pcbs_afectados, i);
            log_warning(kernel_log, "Proceso PID=%d en IO desconectada, moviendo a EXIT", pcb_io_actual->pcb->PID);
            cambiar_estado_pcb(pcb_io_actual->pcb, EXIT_ESTADO);
            free(pcb_io_actual);
        }

        list_destroy(pcbs_afectados);
    }
}