#include "../headers/planificadores.h"
#include <sys/time.h>

pthread_mutex_t mutex_planificador_lp;
pthread_cond_t cond_planificador_lp;
estado_planificador estado_planificador_lp = STOP;

/////////////////////////////// Planificador Corto Plazo ///////////////////////////////
t_pcb* elegir_por_fifo() {
    log_trace(kernel_log, "PLANIFICANDO FIFO");
    log_debug(kernel_log, "FIFO: esperando mutex_cola_ready para elegir proceso FIFO");
    pthread_mutex_lock(&mutex_cola_ready);
    log_debug(kernel_log, "FIFO: bloqueando mutex_cola_ready para elegir proceso FIFO");

    // Se elegira al siguiente proceso a ejecutar segun su orden de llegada a READY.
    if (list_is_empty(cola_ready)) {
        pthread_mutex_unlock(&mutex_cola_ready);
        log_error(kernel_log, "FIFO: cola_ready vacía");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    t_pcb* pcb_fifo = (t_pcb*)list_get(cola_ready, 0);
    pthread_mutex_unlock(&mutex_cola_ready);

    return pcb_fifo;
}

void* menor_rafaga(void* a, void* b) {
    t_pcb* pcb_a = (t_pcb*) a;
    t_pcb* pcb_b = (t_pcb*) b;

    // Devuelve el de menor estimación de ráfaga
    if (pcb_a->estimacion_rafaga < pcb_b->estimacion_rafaga) return pcb_a;
    if (pcb_b->estimacion_rafaga < pcb_a->estimacion_rafaga) return pcb_b;

    // En caso de empate, devolver el primero que llegó (fifo)
    return pcb_a;
}

t_pcb* elegir_por_sjf() {
    log_trace(kernel_log, "PLANIFICANDO SJF (Shortest Job First)");

    log_debug(kernel_log, "SJF: esperando mutex_cola_ready para elegir proceso con menor ráfaga");
    pthread_mutex_lock(&mutex_cola_ready);
    log_debug(kernel_log, "SJF: bloqueando mutex_cola_ready para elegir proceso con menor ráfaga");
    if (list_is_empty(cola_ready)) {
        pthread_mutex_unlock(&mutex_cola_ready);
        log_error(kernel_log, "SJF: cola_ready vacía");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "SJF: buscando entre %d procesos con menor ráfaga en cola_ready", list_size(cola_ready));
    for (int i = 0; i < list_size(cola_ready); i++) {
        mostrar_pcb((t_pcb*)list_get(cola_ready, i));
    }
 
    t_pcb* seleccionado = (t_pcb*)list_get_minimum(cola_ready, menor_rafaga);
    pthread_mutex_unlock(&mutex_cola_ready);

    if (seleccionado) {
        log_trace(kernel_log, "SJF: Proceso elegido PID=%d con estimación=%.2f", 
                  seleccionado->PID, seleccionado->estimacion_rafaga);
    } else {
        log_error(kernel_log, "SJF: No se pudo seleccionar un proceso");
    }

    return seleccionado;
}

t_pcb* elegir_por_srt() {
    log_trace(kernel_log, "elegir_por_srt: PLANIFICANDO SRT (Shortest Remaining Time)");

    log_debug(kernel_log, "elegir_por_srt: esperando mutex_cola_ready para elegir proceso con menor ráfaga restante");
    pthread_mutex_lock(&mutex_cola_ready);
    log_debug(kernel_log, "elegir_por_srt: bloqueando mutex_cola_ready para elegir proceso con menor ráfaga");

    if (list_is_empty(cola_ready)) {
        pthread_mutex_unlock(&mutex_cola_ready);
        log_error(kernel_log, "SRT: cola_ready vacía");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Buscar el proceso READY con menor ráfaga restante
    t_pcb* candidato_ready = (t_pcb*)list_get_minimum(cola_ready, menor_rafaga_restante);
    pthread_mutex_unlock(&mutex_cola_ready);

    if (!candidato_ready) {
        log_error(kernel_log, "SRT: No se pudo seleccionar un proceso READY");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "SRT: Proceso candidato elegido PID=%d con ráfaga restante=%.2f ms", 
              candidato_ready->PID, candidato_ready->estimacion_rafaga);

    log_debug(kernel_log, "SRT: esperando mutex_lista_cpus para buscar CPU disponible o con mayor ráfaga restante");
    pthread_mutex_lock(&mutex_lista_cpus);
    log_debug(kernel_log, "SRT: bloqueando mutex_lista_cpus para buscar CPU disponible o con mayor ráfaga restante");

    bool cpu_libre = false;
    bool cpu_con_mayor_rafaga_restante = false;

    // Buscar CPUs disponibles y calcular cuál ejecuta el proceso con mayor ráfaga restante
    for (int i = 0; i < list_size(lista_cpus); i++) {
        cpu* c = list_get(lista_cpus, i);
        if (c->tipo_conexion != CPU_DISPATCH) continue;

        // Verificar si la CPU está libre (pid = -1)
        if (c->pid == -1) {
            cpu_libre = true;
            break; // hay una CPU libre
        }
    }
    if (!cpu_libre) {
        log_trace(kernel_log, "SRT: No hay CPU libre, verificando ráfagas restantes");
        for (int i = 0; i < list_size(lista_cpus); i++) {
            cpu* c = list_get(lista_cpus, i);
            if (c->tipo_conexion != CPU_DISPATCH) continue;
    
            // Si no está libre, buscar si al menos una tiene mayor ráfaga restante que candidato_ready
            t_pcb* pcb_exec = buscar_pcb(c->pid);
    
            if(menor_rafaga_restante((void*)pcb_exec, (void*)candidato_ready) == (void*)candidato_ready) {
                // Esta CPU tiene un proceso con mayor ráfaga restante que el candidato
                cpu_con_mayor_rafaga_restante = true;
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&mutex_lista_cpus);

    // Si hay CPU libre o hay una CPU ejecutando un proceso con mayor ráfaga restante
    if (cpu_libre) {
        log_trace(kernel_log, "SRT: Hay CPU libre para ejecutar el proceso READY seleccionado (PID=%d)", candidato_ready->PID);
        return candidato_ready; // Se puede asignar el proceso ahora
    } else if(cpu_con_mayor_rafaga_restante) {
        log_trace(kernel_log, "SRT: Hay CPU ocupada con un proceso con mayor ráfaga restante que el proceso READY seleccionado (PID=%d)", candidato_ready->PID);
        return candidato_ready; // Se puede asignar el proceso ahora, pero se deberadesalojar
    } else {        // Si no hay CPU libre ni una ejecutando un proceso con mayor ráfaga restante
        //replanificar cuando haya una cpu libre o entre un proceso en ready?
        log_trace(kernel_log, "SRT: No hay CPU libre ni con mayor ráfaga restante que el proceso READY seleccionado (PID=%d), se reintentará cuando se cumplan las condiciones", candidato_ready->PID);
        return NULL; // No se puede asignar el proceso ahora
    }
}

void* menor_rafaga_restante(void* a, void* b) {
    t_pcb* pcb_a = (t_pcb*) a;
    t_pcb* pcb_b = (t_pcb*) b;

    // Calcular ráfaga restante
    double restante_a;
    double restante_b;
    double ahora = get_time();

    if (pcb_a->tiempo_inicio_exec > 0) {
        restante_a = pcb_a->estimacion_rafaga - (ahora - pcb_a->tiempo_inicio_exec);
    } else {
        restante_a = pcb_a->estimacion_rafaga;
    }

    if (pcb_b->tiempo_inicio_exec > 0) {
        restante_b = pcb_b->estimacion_rafaga - (ahora - pcb_b->tiempo_inicio_exec);
    } else {
        restante_b = pcb_b->estimacion_rafaga;
    }

    // Log de comparación
    log_trace(kernel_log, "Comparando procesos:");
    log_trace(kernel_log, "  • PID %d - Estado: %s - Ráfaga restante: %.2f ms", pcb_a->PID, estado_to_string(pcb_a->Estado), restante_a);
    log_trace(kernel_log, "  • PID %d - Estado: %s - Ráfaga restante: %.2f ms", pcb_b->PID, estado_to_string(pcb_b->Estado), restante_b);

    // Comparar ráfagas restantes
    if (restante_a < restante_b) {
        log_trace(kernel_log,
            "PID %d tiene menor ráfaga restante (%.2f ms) que PID %d (%.2f ms)",
            pcb_a->PID, restante_a, pcb_b->PID, restante_b
        );
        return pcb_a;
    }
    if (restante_b < restante_a) {
        log_trace(kernel_log,
            "PID %d tiene menor ráfaga restante (%.2f ms) que PID %d (%.2f ms)",
            pcb_b->PID, restante_b, pcb_a->PID, restante_a
        );
        return pcb_b;
    }

    // En caso de empate, devolver el primero que llegó (FIFO)
    log_trace(kernel_log,
        "Empate de ráfaga restante entre PID %d y PID %d. Se elige FIFO (PID %d)",
        pcb_a->PID, pcb_b->PID, pcb_a->PID
    );
    return pcb_a;
}


void dispatch(t_pcb* proceso_a_ejecutar) {
    log_trace(kernel_log, "=== DISPATCH INICIADO PARA PID %d ===", proceso_a_ejecutar->PID);

    // Buscar una CPU disponible (con pid = -1 indica que está libre)
    log_debug(kernel_log, "Dispatch: esperando mutex_lista_cpus para buscar CPU disponible");
    pthread_mutex_lock(&mutex_lista_cpus);
    log_debug(kernel_log, "Dispatch: bloqueando mutex_lista_cpus para buscar CPU disponible");

    cpu* cpu_disponible = NULL;
    int total_cpus = list_size(lista_cpus);
    int cpus_dispatch = 0;
    int cpus_libres = 0;

    for (int i = 0; i < total_cpus; i++) {
        cpu* c = list_get(lista_cpus, i);
        if (c->tipo_conexion == CPU_DISPATCH) {
            cpus_dispatch++;
            if (c->pid == -1) {
                cpus_libres++;
                if (!cpu_disponible) {
                    cpu_disponible = c;
                    log_trace(kernel_log, "Dispatch: ✓ CPU %d seleccionada (fd=%d)", c->id, c->fd);
                }
            }
        }
        log_trace(kernel_log, "Dispatch: CPU %d - tipo=%d, pid=%d, fd=%d, estado=%s", 
                  c->id, c->tipo_conexion, c->pid, c->fd, 
                  c->tipo_conexion == CPU_DISPATCH ? (c->pid == -1 ? "LIBRE" : "OCUPADA") : "NO-DISPATCH");
    }

    log_trace(kernel_log, "Dispatch: Total CPUs=%d, CPUs DISPATCH=%d, CPUs libres=%d", total_cpus, cpus_dispatch, cpus_libres);

    if (!cpu_disponible) {
        if(strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0) {
            // Buscar cpu con mayor ráfaga restante
            double max_rafaga_restante = -1;

            for (int i = 0; i < list_size(lista_cpus); i++) {
                cpu* c = list_get(lista_cpus, i);
                if (c->tipo_conexion != CPU_DISPATCH) continue;

                t_pcb* pcb_exec = buscar_pcb(c->pid);
                
                double rafaga_restante;

                if(pcb_exec->tiempo_inicio_exec > 0) {
                    rafaga_restante = pcb_exec->estimacion_rafaga - (get_time() - pcb_exec->tiempo_inicio_exec);
                } else {
                    log_error(kernel_log, "Dispatch: Error al calcular ráfaga restante para PID %d (tiempo_inicio_exec no inicializado)", pcb_exec->PID);
                    terminar_kernel();
                    exit(EXIT_FAILURE);
                }

                if (rafaga_restante > max_rafaga_restante) {
                    max_rafaga_restante = rafaga_restante;
                    cpu_disponible = c;
                }
            }
            // Desalojar
            /*if(!interrupt(cpu_disponible, proceso_a_ejecutar)) {
                log_error(kernel_log, "Dispatch: ✗ Error al enviar interrupción a CPU %d para desalojar PID %d", 
                          cpu_disponible->id, proceso_a_ejecutar->PID);
                terminar_kernel();
                exit(EXIT_FAILURE);
            }
            // Continuar despachando el proceso a la CPU desalojada*/

            t_interrupcion* nueva = malloc(sizeof(t_interrupcion));
            nueva->cpu_a_desalojar = cpu_disponible;
            nueva->pid_a_ejecutar = proceso_a_ejecutar->PID;

            log_debug(kernel_log, "DISPATCH: esperando mutex_cola_interrupciones para encolar interrupción");
            pthread_mutex_lock(&mutex_cola_interrupciones);
            log_debug(kernel_log, "DISPATCH: bloqueando mutex_cola_interrupciones para encolar interrupción");
            queue_push(cola_interrupciones, nueva);
            pthread_mutex_unlock(&mutex_cola_interrupciones);
            log_trace(kernel_log,
                "[INTERRUPT]: Interrupción encolada para desalojar CPU %d (desaloja PID=%d para correr PID=%d)",
                cpu_disponible->id,
                cpu_disponible->pid,  // PID actual en esa CPU
                proceso_a_ejecutar->PID  // nuevo PID a ejecutar
            ); 

            sem_post(&sem_interrupciones);
            log_debug(kernel_log, "[INTERRUPT]: semaforo INTERRUPCIONES aumentado para procesar interrupción");
           
            pthread_mutex_unlock(&mutex_lista_cpus);
            return; // Esperar a que se procese la interrupción

        } else {
            pthread_mutex_unlock(&mutex_lista_cpus);
            log_error(kernel_log, "Dispatch: ✗ No hay CPUs disponibles para ejecutar PID %d", proceso_a_ejecutar->PID);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
    }

    // Marcar CPU como ocupada y guardar PID
    cpu_disponible->pid = proceso_a_ejecutar->PID;
    cpu_disponible->instruccion_actual = EXEC_OP;
    pthread_mutex_unlock(&mutex_lista_cpus);

    // Transicionar a EXEC
    cambiar_estado_pcb(proceso_a_ejecutar, EXEC);

    // Crear y enviar paquete a CPU
    t_paquete* paquete = crear_paquete_op(EXEC_OP);
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PC);
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PID);
    enviar_paquete(paquete, cpu_disponible->fd);
    eliminar_paquete(paquete);

    log_trace(kernel_log, "Dispatch: Proceso %d despachado a CPU %d (PC=%d)", 
              proceso_a_ejecutar->PID, cpu_disponible->id, proceso_a_ejecutar->PC);
}

/*
bool interrupt(cpu* cpu_a_desalojar, t_pcb *proceso_a_ejecutar) {
    log_trace(kernel_log, "Interrupción enviada a CPU %d (fd=%d) para desalojo", cpu_a_desalojar->id, cpu_a_desalojar->fd);
    int fd_interrupt = obtener_fd_interrupt(cpu_a_desalojar->id);

    if (fd_interrupt == -1) {
        log_error(kernel_log, "No se encontró el fd_interrupt para CPU %d", cpu_a_desalojar->id);
        return false;
    }

    // Enviar op code y pid
    t_paquete* paquete = crear_paquete_op(INTERRUPCION_OP);
    enviar_paquete(paquete, fd_interrupt);
    eliminar_paquete(paquete);

    // recibir respuesta
    t_respuesta respuesta;
    if (recv(fd_interrupt, &respuesta, sizeof(t_respuesta), 0) <= 0) {
        log_error(kernel_log, "Error al recibir respuesta de interrupción de CPU %d", cpu_a_desalojar->id);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    
    // Procesar respuesta
    if (respuesta == OK) {
        int buffer_size;
        void* buffer = recibir_buffer(&buffer_size, fd_interrupt);
        if (!buffer) {
            log_error(kernel_log, "Error al recibir buffer de interrupción de CPU %d", cpu_a_desalojar->id);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        // ACtualizar pc del proceso ejecutando, cambiar estado a ready
        int offset = 0;
        if(leer_entero(buffer, &offset) == proceso_a_ejecutar->PID) {
            log_trace(kernel_log, "Interrupción: Proceso %d desalojado de CPU %d", proceso_a_ejecutar->PID, cpu_a_desalojar->id);
            proceso_a_ejecutar->PC = leer_entero(buffer, &offset);
        } else {
            log_error(kernel_log, "Interrupción: PID recibido no coincide con el proceso a desalojar (PID=%d)", proceso_a_ejecutar->PID);
            free(buffer);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        free(buffer);
        return true;
    }
    return false;
}*/

int obtener_fd_interrupt(int id_cpu) {
    // Buscar el fd de la CPU por su ID
    for (int i = 0; i < list_size(lista_cpus); i++) {
        cpu* c = list_get(lista_cpus, i);
        if (c->id == id_cpu && c->tipo_conexion == CPU_INTERRUPT) {
            return c->fd;
        }
    }
    log_error(kernel_log, "obtener_fd_interrupt: No se encontró CPU con ID %d", id_cpu);
    terminar_kernel();
    exit(EXIT_FAILURE);
}

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void* timer_suspension(void* arg) {
    t_pcb* pcb = (t_pcb*)arg;
    usleep(atoi(TIEMPO_SUSPENSION) * 1000);

    if(!pcb) {
        log_trace(kernel_log, AZUL("[PLANI MP] Timer de suspensión ignorado para PID=%d (PCB NULL: el proceso ya ha finalizado)"), pcb->PID);
        return NULL;
    } else if (pcb->tiempo_inicio_blocked < 0 || pcb->Estado != BLOCKED) {
        log_trace(kernel_log, AZUL("[PLANI MP] Timer de suspensión ignorado para PID=%d (Estado=%s, tiempo_inicio_blocked=%.2f)"), 
                  pcb->PID, estado_to_string(pcb->Estado), pcb->tiempo_inicio_blocked);
        return NULL;
    }
        
    log_trace(kernel_log, AZUL("[PLANI MP] Timer de suspensión expirado para PID=%d"), pcb->PID);

    t_paquete* paquete = crear_paquete_op(SUSPENDER_PROCESO_OP);
    agregar_entero_a_paquete(paquete, pcb->PID);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    t_respuesta respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0 || respuesta != OK) {
        log_error(kernel_log, AZUL("[PLANI MP] Error al suspender proceso PID %d"), pcb->PID);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, AZUL("[PLANI MP] Proceso PID %d suspendido correctamente"), pcb->PID);

    cambiar_estado_pcb(pcb, SUSP_BLOCKED);
    // TODO REPLANIFICAR

    return NULL;
}

void iniciar_timer_suspension(t_pcb* pcb) {
    pthread_t hilo_timer;
    if (pthread_create(&hilo_timer, NULL, timer_suspension, pcb) == 0) {
        log_trace(kernel_log, AZUL("[PLANI MP] Hilo de suspensión creado para PID %d"), pcb->PID);
        pthread_detach(hilo_timer);
    } else {
        log_error(kernel_log, AZUL("[PLANI MP] No se pudo crear hilo de suspensión para PID %d"), pcb->PID);
    }
}

void activar_planificador_largo_plazo(void) {
    log_debug(kernel_log, "activar_planificador_largo_plazo: esperando mutex_planificador_lp");
    pthread_mutex_lock(&mutex_planificador_lp);
    log_debug(kernel_log, "activar_planificador_largo_plazo: bloqueando mutex_planificador_lp");
    estado_planificador_lp = RUNNING;
    pthread_cond_signal(&cond_planificador_lp);
    pthread_mutex_unlock(&mutex_planificador_lp);
    log_trace(kernel_log, "Planificador de largo plazo activado");
}

void iniciar_interrupt_handler(void) {
    // Crear hilo para manejar interrupciones
    pthread_t hilo_interrupt_handler;
    if (pthread_create(&hilo_interrupt_handler, NULL, interrupt_handler, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo para manejar interrupciones");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_interrupt_handler);
    log_trace(kernel_log, "Hilo de manejo de interrupciones iniciado correctamente");
}

void* interrupt_handler(void* arg) {
    log_trace(kernel_log, VERDE("Interrupt handler iniciado"));

    while (1) {
        log_debug(kernel_log, "[INTERRUPT]: semaforo INTERRUPCIONES disminuido");
        sem_wait(&sem_interrupciones);

        log_debug(kernel_log, "[INTERRUPT]: esperando mutex_cola_interrupciones");
        pthread_mutex_lock(&mutex_cola_interrupciones);
        log_debug(kernel_log, "[INTERRUPT]: bloqueando mutex_cola_interrupciones");
        t_interrupcion* intr = queue_pop(cola_interrupciones);
        pthread_mutex_unlock(&mutex_cola_interrupciones);

        if (!intr) {
            log_error(kernel_log, VERDE("[INTERRUPT]: Interrupt handler: Cola de interrupción vacía"));
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        int fd_interrupt = obtener_fd_interrupt(intr->cpu_a_desalojar->id);
        if (fd_interrupt == -1) {
            log_error(kernel_log, VERDE("[INTERRUPT]: Interrupt handler: No se encontró fd_interrupt para CPU %d"), intr->cpu_a_desalojar->id);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        log_trace(kernel_log,
            VERDE("[INTERRUPT]: Interrupt handler: Enviando interrupción a CPU %d (desaloja PID=%d para correr PID=%d)"),
            intr->cpu_a_desalojar->id,
            intr->cpu_a_desalojar->pid,
            intr->pid_a_ejecutar
        );        

        // Enviar interrupción con el PID actualmente ejecutando en la CPU
        t_paquete* paquete = crear_paquete_op(INTERRUPCION_OP);
        agregar_entero_a_paquete(paquete, intr->cpu_a_desalojar->pid);
        enviar_paquete(paquete, fd_interrupt);
        eliminar_paquete(paquete);

        // Esperar respuesta
        int respuesta = recibir_operacion(fd_interrupt);

        switch (respuesta) {
            case OK:
                log_trace(kernel_log, VERDE("[INTERRUPT]: CPU %d respondió OK"), intr->cpu_a_desalojar->id);

                t_list* contenido = recibir_contenido_paquete(fd_interrupt);
                if (!contenido) {
                    log_error(kernel_log, "[INTERRUPT]: El contenido recibido es NULL");
                    terminar_kernel();
                    exit(EXIT_FAILURE);
                }
                log_trace(kernel_log, "[INTERRUPT]: Cantidad de elementos en contenido recibido: %d", list_size(contenido));
                
                if (list_size(contenido) < 2) {
                    log_error(kernel_log, "[INTERRUPT]: Error en buffer recibido de CPU");
                    list_destroy_and_destroy_elements(contenido, free);
                    terminar_kernel();
                    exit(EXIT_FAILURE);
                }

                int pid_recibido = *(int*)list_get(contenido, 0);
                int nuevo_pc = *(int*)list_get(contenido, 1);
                list_destroy_and_destroy_elements(contenido, free);

                if (pid_recibido != intr->cpu_a_desalojar->pid) {
                    log_error(kernel_log, "[INTERRUPT]: PID recibido (%d) no coincide con PID esperado (%d)",
                            pid_recibido, intr->cpu_a_desalojar->pid);
                    terminar_kernel();
                    exit(EXIT_FAILURE);
                }

                t_pcb* pcb = buscar_pcb(pid_recibido);

                log_info(kernel_log, MAGENTA("## (%d) - Desalojado por SJF/SRT"), pid_recibido);
                log_trace(kernel_log, "Interrupt handler: Actualizando PCB PID=%d con nuevo PC=%d", pid_recibido, nuevo_pc);

                pcb->PC = nuevo_pc;

                // Limpiar CPU desalojada
                intr->cpu_a_desalojar->pid = -1;
                intr->cpu_a_desalojar->instruccion_actual = -1; // Resetear instrucción actual
                log_trace(kernel_log, "Interrupt handler: CPU %d liberada", intr->cpu_a_desalojar->id);
                cambiar_estado_pcb(pcb, READY);
                solicitar_replanificacion_srt();
                log_trace(kernel_log, "interrupt_handler: replanificacion solicitada");
                free(intr);
                break;
            case ERROR:
                log_trace(kernel_log, VERDE("[INTERRUPT]: Interrupt handler: CPU %d respondió con ERROR"), intr->cpu_a_desalojar->id);
                free(intr);
                break;
            default:
                log_error(kernel_log, "[INTERRUPT]: Interrupt handler: No se pudo recibir respuesta de CPU %d", intr->cpu_a_desalojar->id);
                terminar_kernel();
                exit(EXIT_FAILURE);
        }       
    }
}

void solicitar_replanificacion_srt(void) {

    if(strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0 && list_size(cola_ready) > 0) {
        sem_post(&sem_replanificar_srt);
        log_debug(kernel_log, "solicitar_replanificacion_srt: semaforo replanificar SRT aumentado");
        log_trace(kernel_log, "[PLANI CP]: Replanificación SRT solicitada");
    }
}

void iniciar_planificadores(void) {
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
    if (pthread_create(&hilo_exit, NULL, gestionar_exit, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo para gestionar procesos en EXIT");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_exit);
    
    // Iniciar planificador de corto plazo
    pthread_t hilo_planificador_cp;
    if (pthread_create(&hilo_planificador_cp, NULL, planificador_corto_plazo, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo para planificador de corto plazo");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_planificador_cp);
    log_trace(kernel_log, "Planificador de corto plazo iniciado correctamente");
}

void* planificador_largo_plazo(void* arg) {
    while (1) {
        log_debug(kernel_log, "planificador_largo_plazo: esperando mutex_planificador_lp");
        pthread_mutex_lock(&mutex_planificador_lp);
        log_debug(kernel_log, "planificador_largo_plazo: bloqueando mutex_planificador_lp");
        while (estado_planificador_lp == STOP) {
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
        if(list_size(cola_susp_ready) > 0) {
            log_trace(kernel_log, "planificador_largo_plazo: Hay procesos en SUSP_READY, moviendo a READY");
            // Mover proceso de SUSP_READY a READY
            cola_a_utilizar = cola_susp_ready;
            estado = SUSP_READY;
        } else {
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
        t_pcb* pcb = NULL;
        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0) {
            pcb = (t_pcb*)list_get(cola_a_utilizar, 0);
        } else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0) {
            pcb = elegir_por_pmcp(cola_a_utilizar);
        }

        if (!pcb) {
            log_error(kernel_log, "planificador_largo_plazo: No se pudo obtener un PCB de la cola %s", 
                      estado == NEW ? "NEW" : "SUSP_READY");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        if(estado == SUSP_READY) {
            t_paquete* paquete = crear_paquete_op(DESUSPENDER_PROCESO_OP);
            agregar_entero_a_paquete(paquete, pcb->PID);
            enviar_paquete(paquete, fd_memoria);
            eliminar_paquete(paquete);

            t_respuesta respuesta;
            if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0 || (respuesta != OK && respuesta != ERROR)) {
                log_error(kernel_log, "Error al desuspender proceso PID %d", pcb->PID);
                terminar_kernel();
                exit(EXIT_FAILURE);
            }
            if (respuesta == OK) {
                log_trace(kernel_log, "Desuspensión exitosa del proceso PID %d", pcb->PID);
            }
        } else if (estado == NEW) {
            // Verificar si hay espacio suficiente en memoria
            if (!hay_espacio_suficiente_memoria(pcb->tamanio_memoria)) {
                log_error(kernel_log, "planificador_largo_plazo: No hay espacio suficiente en memoria para el proceso PID %d", pcb->PID);
                continue;
            } else {
                // Comunicarse con memoria para inicializar el proceso
                t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
                agregar_a_paquete(paquete, &pcb->PID, sizeof(int));
                agregar_a_paquete(paquete, &pcb->path, strlen(pcb->path) + 1);
                agregar_a_paquete(paquete, &pcb->tamanio_memoria, sizeof(int));
                enviar_paquete(paquete, fd_memoria);
                eliminar_paquete(paquete);
                // Esperar respuesta de memoria
                t_respuesta respuesta;
                if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0 || (respuesta != OK && respuesta != ERROR)) {
                    log_error(kernel_log, "Error al recibir respuesta de memoria para INIT_PROC");
                    terminar_kernel();
                    exit(EXIT_FAILURE);
                }
                
                // Procesar respuesta
                if (respuesta == OK) {
                    log_trace(kernel_log, "planificador_largo_plazo: Proceso PID %d inicializado correctamente en memoria", pcb->PID);
                } else {
                    log_trace(kernel_log, "planificador_largo_plazo: Error al inicializar proceso PID %d en memoria", pcb->PID);
                    free(pcb);
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

void* menor_tamanio(void* a, void* b) {
    t_pcb* pcb_a = (t_pcb*) a;
    t_pcb* pcb_b = (t_pcb*) b;
    return pcb_a->tamanio_memoria <= pcb_b->tamanio_memoria ? pcb_a : pcb_b;
}

t_pcb* elegir_por_pmcp(t_list *cola_a_utilizar) {
    log_trace(kernel_log, "PLANIFICANDO PMCP (Proceso Mas Chico Primero)");
    t_pcb* pcb_mas_chico = (t_pcb*)list_get_minimum(cola_a_utilizar, menor_tamanio);
    if (!pcb_mas_chico) {
        log_error(kernel_log, "elegir_por_pmcp: No se encontró ningún proceso en NEW o SUSP_READY");
        terminar_kernel();
        exit(EXIT_FAILURE);
    } else {
        log_trace(kernel_log, "elegir_por_pmcp: Proceso elegido PID=%d, Tamaño=%d", pcb_mas_chico->PID, pcb_mas_chico->tamanio_memoria);
    }
    return (t_pcb*)pcb_mas_chico;
}

void* gestionar_exit(void* arg) {
    while (1) {
        log_debug(kernel_log, "gestionar_exit: Semaforo a EXIT disminuido");
        sem_wait(&sem_proceso_a_exit);
        log_debug(kernel_log, "gestionar_exit: esperando mutex_cola_exit para procesar EXIT");
        pthread_mutex_lock(&mutex_cola_exit);
        log_debug(kernel_log, "gestionar_exit: bloqueando mutex_cola_exit para procesar EXIT");
        if (list_is_empty(cola_exit)) {
            pthread_mutex_unlock(&mutex_cola_exit);
            log_error(kernel_log, "gestionar_exit: Se despertó pero no hay procesos en EXIT");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        t_pcb* pcb = list_get(cola_exit, 0);
        pthread_mutex_unlock(&mutex_cola_exit);

        if (!pcb) {
            log_error(kernel_log, "gestionar_exit: No se pudo obtener PCB desde EXIT");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        log_trace(kernel_log, "gestionar_exit: Ejecutando syscall EXIT para PID=%d", pcb->PID);
        EXIT(pcb);
    }

    return NULL;
}

void* planificador_corto_plazo(void* arg) {
    log_trace(kernel_log, "=== PLANIFICADOR CP INICIADO ===");
    
    while (1) {

        if(strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0) {
            log_debug(kernel_log, "Planificador CP: semaforo REPLANIFICAR SRT disminuido");
            sem_wait(&sem_replanificar_srt);
        } else {    
            // Esperar a que llegue un proceso a READY
            log_debug(kernel_log, "planificador_corto_plazo: Semaforo a READY disminuido");
            sem_wait(&sem_proceso_a_ready);   
            // Esperar cpu disponible
            log_trace(kernel_log, "Planificador CP: ✓ Proceso llegó a READY - Verificando disponibilidad de cpu");
            log_debug(kernel_log, "planificador_corto_plazo: Semaforo CPU DISPONIBLE disminuido");
            sem_wait(&sem_cpu_disponible);
        }
        log_trace(kernel_log, "Planificador CP: ✓ Condiciones cumplidas - Iniciando planificación");
        t_pcb* proceso_elegido;

        if (strcmp(ALGORITMO_CORTO_PLAZO, "FIFO") == 0) {
            proceso_elegido = elegir_por_fifo();
        } else if (strcmp(ALGORITMO_CORTO_PLAZO, "SJF") == 0) {
            proceso_elegido = elegir_por_sjf();
        } else if (strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0) {
            proceso_elegido = elegir_por_srt();
        }
        else {
            log_error(kernel_log, "iniciar_planificador_corto_plazo: Algoritmo no reconocido");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        if (proceso_elegido) {
            dispatch(proceso_elegido);
            continue;
        } else if(strcmp(ALGORITMO_CORTO_PLAZO, "SRT") != 0) {
            log_error(kernel_log, "planificador_corto_plazo: No se pudo elegir un proceso para ejecutar");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        log_trace(kernel_log, "planificador_corto_plazo: No se pudo elegir un proceso para ejecutar para SRT");
    }
}

bool hay_espacio_suficiente_memoria(int tamanio) {
    t_paquete* paquete = crear_paquete_op(CHECK_MEMORY_SPACE_OP);
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    t_paquete* respuesta = (t_paquete*)recibir_paquete(fd_memoria);
    if (respuesta == NULL) {
        log_error(kernel_log, "Error al recibir respuesta de memoria");
        return false;
    }
    
    // Convertimos el código de operación a t_resultado_memoria
    t_resultado_memoria resultado = (t_resultado_memoria)respuesta->codigo_operacion;
    bool hay_espacio = resultado == MEMORIA_OK;
    eliminar_paquete(respuesta);
    return hay_espacio;
}

