#include "../headers/planificadores.h"

void activar_planificador_largo_plazo(void)
{
    log_debug(kernel_log, "activar_planificador_largo_plazo: esperando mutex_planificador_lp");
    pthread_mutex_lock(&mutex_planificador_lp);
    log_debug(kernel_log, "activar_planificador_largo_plazo: bloqueando mutex_planificador_lp");
    estado_planificador_lp = RUNNING;
    pthread_cond_signal(&cond_planificador_lp);
    pthread_mutex_unlock(&mutex_planificador_lp);
    log_trace(kernel_log, "Planificador de largo plazo activado");
}
void *planificador_largo_plazo(void *arg)
{
    while (1)
    {
        log_debug(kernel_log, "planificador_largo_plazo: esperando mutex_planificador_lp");
        pthread_mutex_lock(&mutex_planificador_lp);
        log_debug(kernel_log, "planificador_largo_plazo: bloqueando mutex_planificador_lp");
        while (estado_planificador_lp == STOP)
        {
            log_trace(kernel_log, "Planificador de largo plazo en STOP, esperando activación...");
            pthread_cond_wait(&cond_planificador_lp, &mutex_planificador_lp);
        }
        pthread_mutex_unlock(&mutex_planificador_lp);

        // Esperar procesos en NEW
        log_debug(kernel_log, "planificador_largo_plazo: Semaforo a NEW disminuido");
        sem_wait(&sem_proceso_a_new);
        // TODO REPLANIFICACION AL LIBERAR MEMORIA

        t_list *cola_a_utilizar;
        Estados estado;
        log_debug(kernel_log, "planificador_largo_plazo: esperando mutex_cola_susp_ready");
        pthread_mutex_lock(&mutex_cola_susp_ready);
        log_debug(kernel_log, "planificador_largo_plazo: bloqueando mutex_cola_susp_ready");
        if (list_size(cola_susp_ready) > 0)
        {
            log_trace(kernel_log, "planificador_largo_plazo: Hay procesos en SUSP_READY, moviendo a READY");
            // Mover proceso de SUSP_READY a READY
            cola_a_utilizar = cola_susp_ready;
            estado = SUSP_READY;
        }
        else
        {
            log_trace(kernel_log, "planificador_largo_plazo: No hay procesos en SUSP_READY, utilizando cola_new");
            // Utilizar cola_new
            cola_a_utilizar = cola_new;
            estado = NEW;
        }
        pthread_mutex_unlock(&mutex_cola_susp_ready);

        // Obtener el proceso de NEW según el algoritmo
        log_trace(kernel_log, "planificador_largo_plazo: esperando mutex de cola_new o cola_susp_ready");
        bloquear_cola_por_estado(estado);
        log_trace(kernel_log, "planificador_largo_plazo: Bloqueando mutex de cola_new o cola_susp_ready");
        t_pcb *pcb = NULL;
        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0)
        {
            pcb = (t_pcb *)list_get(cola_a_utilizar, 0);
        }
        else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0)
        {
            pcb = elegir_por_pmcp(cola_a_utilizar);
        }

        if (!pcb)
        {
            log_error(kernel_log, "planificador_largo_plazo: No se pudo obtener un PCB de la cola %s",
                      estado == NEW ? "NEW" : "SUSP_READY");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        if (estado == SUSP_READY)
        {
            t_paquete *paquete = crear_paquete_op(DESUSPENDER_PROCESO_OP);
            agregar_entero_a_paquete(paquete, pcb->PID);
            enviar_paquete(paquete, fd_memoria);
            eliminar_paquete(paquete);

            t_respuesta respuesta;
            if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0 || (respuesta != OK && respuesta != ERROR))
            {
                log_error(kernel_log, "Error al desuspender proceso PID %d", pcb->PID);
                terminar_kernel();
                exit(EXIT_FAILURE);
            }
            if (respuesta == OK)
            {
                log_trace(kernel_log, "Desuspensión exitosa del proceso PID %d", pcb->PID);
            }
        }
        else if (estado == NEW)
        {
            // Verificar si hay espacio suficiente en memoria
            if (!hay_espacio_suficiente_memoria(pcb->tamanio_memoria))
            {
                log_debug(kernel_log, "planificador_largo_plazo: No hay espacio suficiente en memoria para el proceso PID %d", pcb->PID);
                liberar_cola_por_estado(estado);
                continue;
            }
            else
            {
                // Comunicarse con memoria para inicializar el proceso
                log_trace(kernel_log, "planificador_largo_plazo: Pedido de alojamiento a memoria para PID %d", pcb->PID);
                t_paquete *paquete = crear_paquete_op(INIT_PROC_OP);
                agregar_a_paquete(paquete, &pcb->PID, sizeof(int));
                agregar_a_paquete(paquete, pcb->path, strlen(pcb->path) + 1);
                agregar_a_paquete(paquete, &pcb->tamanio_memoria, sizeof(int));
                enviar_paquete(paquete, fd_memoria);
                eliminar_paquete(paquete);
                // Esperar respuesta de memoria
                t_respuesta respuesta;
                if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0 || (respuesta != OK && respuesta != ERROR))
                {
                    log_error(kernel_log, "Error al recibir respuesta de memoria para INIT_PROC");
                    liberar_cola_por_estado(estado);
                    terminar_kernel();
                    exit(EXIT_FAILURE);
                }

                // Procesar respuesta
                if (respuesta == OK)
                {
                    log_trace(kernel_log, "planificador_largo_plazo: Proceso PID %d inicializado correctamente en memoria", pcb->PID);
                }
                else
                {
                    log_trace(kernel_log, "planificador_largo_plazo: Error al inicializar proceso PID %d en memoria", pcb->PID);
                    liberar_cola_por_estado(estado);
                    continue; // No se pudo inicializar, esperar una replanificacion
                }
            }
        }
        liberar_cola_por_estado(estado);

        log_trace(kernel_log, "planificador_largo_plazo: enviando un nuevo proceso a READY");
        cambiar_estado_pcb(pcb, READY);
    }
    return NULL;
}

void *menor_tamanio(void *a, void *b)
{
    t_pcb *pcb_a = (t_pcb *)a;
    t_pcb *pcb_b = (t_pcb *)b;
    return pcb_a->tamanio_memoria <= pcb_b->tamanio_memoria ? pcb_a : pcb_b;
}

t_pcb *elegir_por_pmcp(t_list *cola_a_utilizar)
{
    log_trace(kernel_log, "PLANIFICANDO PMCP (Proceso Mas Chico Primero)");
    t_pcb *pcb_mas_chico = (t_pcb *)list_get_minimum(cola_a_utilizar, menor_tamanio);
    if (!pcb_mas_chico)
    {
        log_error(kernel_log, "elegir_por_pmcp: No se encontró ningún proceso en NEW o SUSP_READY");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    else
    {
        log_trace(kernel_log, "elegir_por_pmcp: Proceso elegido PID=%d, Tamaño=%d", pcb_mas_chico->PID, pcb_mas_chico->tamanio_memoria);
    }
    return (t_pcb *)pcb_mas_chico;
}

void *gestionar_exit(void *arg)
{
    while (1)
    {
        log_debug(kernel_log, "gestionar_exit: Semaforo a EXIT disminuido");
        sem_wait(&sem_proceso_a_exit);
        log_debug(kernel_log, "gestionar_exit: esperando mutex_cola_exit para procesar EXIT");
        pthread_mutex_lock(&mutex_cola_exit);
        log_debug(kernel_log, "gestionar_exit: bloqueando mutex_cola_exit para procesar EXIT");
        if (list_is_empty(cola_exit))
        {
            pthread_mutex_unlock(&mutex_cola_exit);
            log_error(kernel_log, "gestionar_exit: Se despertó pero no hay procesos en EXIT");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        t_pcb *pcb = list_get(cola_exit, 0);
        pthread_mutex_unlock(&mutex_cola_exit);

        if (!pcb)
        {
            log_error(kernel_log, "gestionar_exit: No se pudo obtener PCB desde EXIT");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        log_trace(kernel_log, "gestionar_exit: Ejecutando syscall EXIT para PID=%d", pcb->PID);
        EXIT(pcb);
    }

    return NULL;
}

bool hay_espacio_suficiente_memoria(int tamanio)
{
    log_trace(kernel_log, "Verificando espacio suficiente en memoria para tamaño %d", tamanio);
    t_paquete *paquete = crear_paquete_op(CHECK_MEMORY_SPACE_OP);
    agregar_entero_a_paquete(paquete, tamanio);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    int respuesta = recibir_operacion(fd_memoria);
    if (respuesta < 0 || (respuesta != OK && respuesta != ERROR))
    {
        log_error(kernel_log, "Error al recibir respuesta de memoria");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    if (respuesta == OK)
    {
        log_trace(kernel_log, "Espacio suficiente en memoria para tamaño %d", tamanio);
        return true;
    }
    else if (respuesta == ERROR)
    {
        log_trace(kernel_log, "No hay espacio suficiente en memoria para tamaño %d", tamanio);
        return false;
    } else {
        log_error(kernel_log, "Respuesta inesperada de memoria: %d", respuesta);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
}
