#include "../headers/planificadores.h"
#include <sys/time.h>

/////////////////////////////// Planificador Corto Plazo ///////////////////////////////
t_pcb* elegir_por_fifo() {
    log_trace(kernel_log, "PLANIFICANDO FIFO");

    // Se elegira al siguiente proceso a ejecutar segun su orden de llegada a READY.
    return (t_pcb*)list_get(cola_ready, 0);
}

void* menor_rafaga(void* a, void* b) {
    t_pcb* pcb_a = (t_pcb*) a;
    t_pcb* pcb_b = (t_pcb*) b;
    return pcb_a->estimacion_rafaga <= pcb_b->estimacion_rafaga ? pcb_a : pcb_b;
}
t_pcb* elegir_por_sjf() {
    log_trace(kernel_log, "PLANIFICANDO SJF");

    /*  Se elegira el proceso que tenga la rafaga mas corta.
        Su funcionamiento sera como se explica en teoria y la funcion de como calcular las rafagas es la siguiente
    
        Est(n) = Estimado de la rafaga anterior
        R(n) = Lo que realmente ejecuto de la rafaga anterior en la CPU

        Est(n+1) = El estimado de la proxima rafaga
        Est(n+1) =  R(n) + (1-) Est(n) ;     [0,1]
    */

    return (t_pcb*)list_get_minimum(cola_ready, menor_rafaga); // Elige al PCB con la menor ESTIMACIoN de rafaga
}

t_pcb* elegir_por_srt() {
    log_trace(kernel_log, "PLANIFICANDO SRT");

    /*
        Funciona igual que el anterior con la variante que al ingresar un proceso en la cola de Ready
        existiendo al menos un proceso en Exec, se debe evaluar si dicho proceso tiene una rafaga mas corta que 
        los que se encuentran en ejecucion. En caso de ser asi, se debe informar al CPU que posee al Proceso 
        con el tiempo mas alto que debe desalojar al mismo para que pueda ser planificado el nuevo.
    

    pthread_t hilo_algoritmo_srt;
    pthread_create(&hilo_algoritmo_srt, NULL, chequear_ready, NULL);
    pthread_detach(hilo_algoritmo_srt);
    */
    //t_pcb* menor_rafaga = list_get_minimum(cola_ready, menor_rafaga);

    return (t_pcb*)list_get(cola_ready, 0);
}

void dispatch(t_pcb* proceso_a_ejecutar) {
    log_trace(kernel_log, "=== DISPATCH INICIADO PARA PID %d ===", proceso_a_ejecutar->PID);

    // Buscar una CPU disponible (con pid = -1 indica que está libre)
    pthread_mutex_lock(&mutex_lista_cpus);
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
        pthread_mutex_unlock(&mutex_lista_cpus);
        log_error(kernel_log, "Dispatch: ✗ No hay CPUs disponibles para ejecutar PID %d", proceso_a_ejecutar->PID);
        // TODO: Manejar caso sin CPUs disponibles (ej: reencolar o retry)
        return;
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
    agregar_a_paquete(paquete, &proceso_a_ejecutar->PC, sizeof(int));
    agregar_a_paquete(paquete, &proceso_a_ejecutar->PID, sizeof(int));
    enviar_paquete(paquete, cpu_disponible->fd);
    eliminar_paquete(paquete);

    log_trace(kernel_log, "Dispatch: Proceso %d despachado a CPU %d (PC=%d)", 
              proceso_a_ejecutar->PID, cpu_disponible->id, proceso_a_ejecutar->PC);
}

void iniciar_planificador_corto_plazo(char* algoritmo) {
    t_pcb* proceso_elegido;

    if (!list_is_empty(cola_ready) && strcmp(algoritmo, "FIFO") == 0) {
        proceso_elegido = elegir_por_fifo();
    } else if (!list_is_empty(cola_ready) && strcmp(algoritmo, "SJF") == 0) {
        proceso_elegido = elegir_por_sjf();
    } else if (!list_is_empty(cola_ready) && strcmp(algoritmo, "SRT") == 0) {
        proceso_elegido = elegir_por_srt();
    }
    else if (list_is_empty(cola_ready)) {
        log_error(kernel_log, "iniciar_planificador_corto_plazo: Cola READY vacia");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    else {
        log_error(kernel_log, "iniciar_planificador_corto_plazo: Algoritmo no reconocido");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    dispatch(proceso_elegido);
}

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

//////////////////////////// Planificacion de Largo Plazo ////////////////////////////

// Add enum for planificador states
typedef enum {
    STOP,
    RUNNING
} estado_planificador;

// Add global variables for planificador
static pthread_mutex_t mutex_planificador_lp;
static pthread_cond_t cond_planificador_lp;
static estado_planificador estado_planificador_lp = STOP;

// Declaración de la función antes de usarla
static void* planificador_largo_plazo(void* arg);

void activar_planificador_largo_plazo(void) {
    pthread_mutex_lock(&mutex_planificador_lp);
    estado_planificador_lp = RUNNING;
    pthread_cond_signal(&cond_planificador_lp);
    pthread_mutex_unlock(&mutex_planificador_lp);
    log_trace(kernel_log, "Planificador de largo plazo activado");
}

void iniciar_planificador_largo_plazo(void) {
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
    
    // Iniciar planificador de corto plazo reactivo
    pthread_t hilo_planificador_cp;
    if (pthread_create(&hilo_planificador_cp, NULL, planificador_corto_plazo_reactivo, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo para planificador de corto plazo reactivo");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_planificador_cp);
    log_trace(kernel_log, "Planificador de corto plazo reactivo iniciado correctamente");
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
        pthread_mutex_lock(&mutex_cola_new);
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
    pthread_mutex_lock(&mutex_cola_new);
    t_pcb* pcb_mas_chico = (t_pcb*)list_get_minimum(cola_new, menor_tamanio);
    pthread_mutex_unlock(&mutex_cola_new);
    return (t_pcb*)pcb_mas_chico;
}

void* gestionar_exit(void* arg) {
    while (1) {
        log_debug(kernel_log, "gestionar_exit: Semaforo a EXIT disminuido");
        sem_wait(&sem_proceso_a_exit);

        pthread_mutex_lock(&mutex_cola_exit);
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

// NUEVO: Planificador de corto plazo reactivo
void* planificador_corto_plazo_reactivo(void* arg) {
    log_trace(kernel_log, "=== PLANIFICADOR CP REACTIVO INICIADO ===");
    
    while (1) {
        log_trace(kernel_log, "Planificador CP reactivo: Esperando semáforo sem_proceso_a_ready...");
        
        // Esperar a que llegue un proceso a READY
        log_debug(kernel_log, "planificador_corto_plazo_reactivo: Semaforo a READY disminuido");
        sem_wait(&sem_proceso_a_ready);
        
        log_trace(kernel_log, "Planificador CP reactivo: ✓ Proceso llegó a READY - Iniciando evaluación");
        
        // Verificar si hay CPUs disponibles Y procesos en READY
        pthread_mutex_lock(&mutex_lista_cpus);
        bool hay_cpu_disponible = false;
        int cpus_totales = list_size(lista_cpus);
        int cpus_dispatch = 0;
        int cpus_libres = 0;
        
        for (int i = 0; i < list_size(lista_cpus); i++) {
            cpu* c = list_get(lista_cpus, i);
            if (c->tipo_conexion == CPU_DISPATCH) {
                cpus_dispatch++;
                if (c->pid == -1) {
                    hay_cpu_disponible = true;
                    cpus_libres++;
                }
            }
        }
        pthread_mutex_unlock(&mutex_lista_cpus);
        
        log_trace(kernel_log, "Planificador CP reactivo: CPUs totales=%d, CPUs dispatch=%d, CPUs libres=%d", 
                 cpus_totales, cpus_dispatch, cpus_libres);
        
        // Solo planificar si hay CPU disponible Y cola READY no vacía
        pthread_mutex_lock(&mutex_cola_ready);
        bool hay_procesos_ready = !list_is_empty(cola_ready);
        int procesos_en_ready = list_size(cola_ready);
        pthread_mutex_unlock(&mutex_cola_ready);
        
        log_trace(kernel_log, "Planificador CP reactivo: Procesos en READY=%d, hay_cpu_disponible=%s", 
                 procesos_en_ready, hay_cpu_disponible ? "SÍ" : "NO");
        
        if (hay_cpu_disponible && hay_procesos_ready) {
            log_trace(kernel_log, "Planificador CP reactivo: ✓ Condiciones cumplidas - Iniciando planificación");
            iniciar_planificador_corto_plazo(ALGORITMO_CORTO_PLAZO);
            log_trace(kernel_log, "Planificador CP reactivo: ✓ Planificación completada");
        } else {
            if (!hay_cpu_disponible) {
                log_trace(kernel_log, "Planificador CP reactivo: ⚠ No hay CPUs disponibles");
            }
            if (!hay_procesos_ready) {
                log_trace(kernel_log, "Planificador CP reactivo: ⚠ Cola READY vacía");
            }
            log_trace(kernel_log, "Planificador CP reactivo: Reposteando semáforo para reintento posterior");
            // Re-postear el semáforo para que se pueda reintentar cuando cambien las condiciones
            sem_post(&sem_proceso_a_ready);
            log_debug(kernel_log, "planificador_corto_plazo_reactivo: Semaforo a READY aumentado");
        }
    }
    
    return NULL;
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

