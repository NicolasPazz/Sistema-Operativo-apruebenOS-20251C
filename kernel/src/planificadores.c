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

    log_debug(kernel_log, "SJF: buscando entre %d procesos con menor ráfaga en cola_ready", list_size(cola_ready));
    for (int i = 0; i < list_size(cola_ready); i++) {
        mostrar_pcb((t_pcb*)list_get(cola_ready, i));
    }
 
    t_pcb* seleccionado = (t_pcb*)list_get_minimum(cola_ready, menor_rafaga);
    pthread_mutex_unlock(&mutex_cola_ready);

    if (seleccionado) {
        log_debug(kernel_log, "SJF: Proceso elegido PID=%d con estimación=%.2f", 
                  seleccionado->PID, seleccionado->estimacion_rafaga);
    } else {
        log_error(kernel_log, "SJF: No se pudo seleccionar un proceso");
    }

    return seleccionado;
}

t_pcb* elegir_por_srt() {
    log_trace(kernel_log, "PLANIFICANDO SRT (Shortest Remaining Time)");

    pthread_mutex_lock(&mutex_cola_ready);
    if (list_is_empty(cola_ready)) {
        pthread_mutex_unlock(&mutex_cola_ready);
        log_warning(kernel_log, "SRT: cola_ready vacía, no hay proceso para planificar");
        return NULL;
    }

    // Buscar el proceso READY con menor ráfaga restante
    t_pcb* candidato_ready = (t_pcb*)list_get_minimum(cola_ready, menor_rafaga_restante);
    pthread_mutex_unlock(&mutex_cola_ready);

    if (!candidato_ready) {
        log_warning(kernel_log, "SRT: No se pudo seleccionar un proceso READY");
        return NULL;
    }

    pthread_mutex_lock(&mutex_lista_cpus);

    bool cpu_libre = false;
    bool cpu_con_mayor_rafaga_restante = false;

    for (int i = 0; i < list_size(lista_cpus); i++) {
        cpu* c = list_get(lista_cpus, i);
        if (c->tipo_conexion != CPU_DISPATCH) continue;

        // Verificar si la CPU está libre (pid = -1)
        if (c->pid == -1) {
            cpu_libre = true;
            break; // hay una CPU libre
        }

        // Si no está libre, buscar si al menos una tiene mayor ráfaga restante que candidato_ready
        t_pcb* pcb_exec = buscar_pcb(c->pid);
        if (!pcb_exec) {
            log_warning(kernel_log, "SRT: No se encontró PCB de la CPU con PID %d (puede haber terminado)", c->pid);
            continue;
        }

        if(menor_rafaga_restante((void*)pcb_exec, (void*)candidato_ready) == (void*)candidato_ready) {
            // Esta CPU tiene un proceso con mayor ráfaga restante que el candidato
            cpu_con_mayor_rafaga_restante = true;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_lista_cpus);

    if (cpu_libre || cpu_con_mayor_rafaga_restante) {
        log_trace(kernel_log, "SRT: Hay CPU libre o con mayor ráfaga restante, proceso seleccionado PID=%d", candidato_ready->PID);
        return candidato_ready;
    } else {
        log_trace(kernel_log, "SRT: No hay CPU libre ni una con mayor ráfaga restante. El proceso queda esperando en READY.");
        // No se elimina ni termina el proceso, solo se devuelve NULL para que el planificador espere.
        return NULL;
    }
}
void* menor_rafaga_restante(void* a, void* b) {
    t_pcb* pcb_a = (t_pcb*) a;
    t_pcb* pcb_b = (t_pcb*) b;

    // Calcular ráfaga restante
    double restante_a;
    double restante_b;

    if(pcb_a->tiempo_inicio_exec > 0) {
        restante_a = pcb_a->estimacion_rafaga - (get_time() - pcb_a->tiempo_inicio_exec);
    } else {
        restante_a = pcb_a->estimacion_rafaga;
    }
    if(pcb_b->tiempo_inicio_exec > 0) {
        restante_b = pcb_b->estimacion_rafaga - (get_time() - pcb_b->tiempo_inicio_exec);
    } else {
        restante_b = pcb_b->estimacion_rafaga;
    }

    // Comparar ráfagas restantes
    if (restante_a < restante_b) return pcb_a;
    if (restante_b < restante_a) return pcb_b;

    // En caso de empate, devolver el primero que llegó (FIFO)
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
                if (!pcb_exec) {
                    log_error(kernel_log, "Error al obtener PCB de la CPU con PID %d", c->pid);
                    terminar_kernel();
                    exit(EXIT_FAILURE);
                }
                
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
            if(!interrupt(cpu_disponible, proceso_a_ejecutar)){
                // TODO: replanificar cuando haya una cpu libre o entre un proceso en ready?
                exit(EXIT_FAILURE);
            }
            // Continuar despachando el proceso a la CPU desalojada
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
    proceso_a_ejecutar->tiempo_inicio_exec = get_time();

    // Crear y enviar paquete a CPU
    t_paquete* paquete = crear_paquete_op(EXEC_OP);
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PC);
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PID);
    enviar_paquete(paquete, cpu_disponible->fd);
    eliminar_paquete(paquete);

    log_trace(kernel_log, "Dispatch: Proceso %d despachado a CPU %d (PC=%d)", 
              proceso_a_ejecutar->PID, cpu_disponible->id, proceso_a_ejecutar->PC);
}

bool interrupt(cpu* cpu_a_desalojar, t_pcb *proceso_a_ejecutar) {
    log_trace(kernel_log, "Interrupción enviada a CPU %d (fd=%d) para desalojo", cpu_a_desalojar->id, cpu_a_desalojar->fd);
    //int fd_interrupt = obtener_fd_interrupt(cpu_a_desalojar->id);

    // Enviar op code y pid
    t_paquete* paquete = crear_paquete_op(INTERRUPCION_OP);
    enviar_paquete(paquete, fd_interrupt);
    eliminar_paquete(paquete);

    // recibir respuesta
    t_respuesta respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0) {
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
        return true;
    }
    return false;
}

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void activar_planificador_largo_plazo(void) {
    pthread_mutex_lock(&mutex_planificador_lp);
    estado_planificador_lp = RUNNING;
    pthread_cond_signal(&cond_planificador_lp);
    pthread_mutex_unlock(&mutex_planificador_lp);
    log_trace(kernel_log, "Planificador de largo plazo activado");
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
        pthread_mutex_lock(&mutex_planificador_lp);
        while (estado_planificador_lp == STOP) {
            log_trace(kernel_log, "Planificador de largo plazo en STOP, esperando activación...");
            pthread_cond_wait(&cond_planificador_lp, &mutex_planificador_lp);
        }
        pthread_mutex_unlock(&mutex_planificador_lp);

        // Esperar procesos en NEW
        log_debug(kernel_log, "planificador_largo_plazo: Semaforo a NEW disminuido");
        sem_wait(&sem_proceso_a_new);

        log_trace(kernel_log, "planificador_largo_plazo: Hay procesos en NEW, intentando mover a READY");
            
        // Esperar a que cola_susp_ready esté vacía
        //log_debug(kernel_log, "planificador_largo_plazo: Semaforo SUSP READY VACIA disminuido");
        //sem_wait(&sem_susp_ready_vacia);
            
        // Obtener el proceso de NEW según el algoritmo
        log_debug(kernel_log, "planificador_largo_plazo: esperando mutex_cola_new para obtener PCB de NEW");
        pthread_mutex_lock(&mutex_cola_new);
        log_debug(kernel_log, "planificador_largo_plazo: Bloqueando mutex_cola_new para obtener PCB de NEW");
        t_pcb* pcb = NULL;
        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0) {
            pcb = (t_pcb*)list_get(cola_new, 0);
        } else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0) {
            pcb = elegir_por_pmcp();
        }

        pthread_mutex_unlock(&mutex_cola_new);

        if (pcb) {
            log_debug(kernel_log, "planificador_largo_plazo: enviando un nuevo proceso a READY");
            cambiar_estado_pcb(pcb, READY);
        } else {
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
           
        //sem_post(&sem_susp_ready_vacia);
        //log_debug(kernel_log, "planificador_largo_plazo: Semaforo SUSP READY VACIA aumentado");

    }
    return NULL;
}

void* menor_tamanio(void* a, void* b) {
    t_pcb* pcb_a = (t_pcb*) a;
    t_pcb* pcb_b = (t_pcb*) b;
    return pcb_a->tamanio_memoria <= pcb_b->tamanio_memoria ? pcb_a : pcb_b;
}

t_pcb* elegir_por_pmcp() {
    log_trace(kernel_log, "PLANIFICANDO PMCP (Proceso Mas Chico Primero)");
    t_pcb* pcb_mas_chico = (t_pcb*)list_get_minimum(cola_new, menor_tamanio);
    if (!pcb_mas_chico) {
        log_error(kernel_log, "elegir_por_pmcp: No se encontró ningún proceso en NEW");
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
        log_trace(kernel_log, "Planificador CP: Esperando semáforo sem_proceso_a_ready...");
        
        // Esperar a que llegue un proceso a READY
        log_debug(kernel_log, "planificador_corto_plazo: Semaforo a READY disminuido");
        sem_wait(&sem_proceso_a_ready);
        
        log_trace(kernel_log, "Planificador CP: ✓ Proceso llegó a READY - Verificando disponibilidad de cpu");
        
        // Esperar cpu disponible
        log_debug(kernel_log, "planificador_corto_plazo: Semaforo CPU DISPONIBLE disminuido");
        sem_wait(&sem_cpu_disponible);
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

        dispatch(proceso_elegido);
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

