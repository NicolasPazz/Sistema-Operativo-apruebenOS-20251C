#include "../headers/planificadores.h"
#include <sys/time.h>

/////////////////////////////// Planificador Corto Plazo ///////////////////////////////
t_pcb* elegir_por_fifo(){
    log_trace(kernel_log, "PLANIFICANDO FIFO");

    // Se elegira al siguiente proceso a ejecutar segun su orden de llegada a READY.
    return (t_pcb*)list_get(cola_ready, 0);
}

void* menor_rafaga(void* a, void* b) {
    t_pcb* pcb_a = (t_pcb*) a;
    t_pcb* pcb_b = (t_pcb*) b;
    return pcb_a->estimacion_rafaga <= pcb_b->estimacion_rafaga ? pcb_a : pcb_b;
}
t_pcb* elegir_por_sjf(){
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

t_pcb* elegir_por_srt(){
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

void dispatch(t_pcb* proceso_a_ejecutar){
    log_info(kernel_log, "=== DISPATCH INICIADO PARA PID %d ===", proceso_a_ejecutar->PID);

    // Una vez seleccionado el proceso a ejecutar, se lo transicionara al estado EXEC
    cambiar_estado_pcb(proceso_a_ejecutar, EXEC);
    proceso_a_ejecutar->tiempo_inicio_exec = get_time();

    // CAMBIO: Implementar envío real al CPU
    log_trace(kernel_log, "Enviando PID %d a CPU por Dispatch para que lo ejecute", proceso_a_ejecutar->PID);
    
    // Buscar una CPU disponible (con pid = -1 indica que está libre)
    pthread_mutex_lock(&mutex_lista_cpus);
    cpu* cpu_disponible = NULL;
    log_debug(kernel_log, "Dispatch: Buscando CPU disponible entre %d CPUs...", list_size(lista_cpus));
    
    for (int i = 0; i < list_size(lista_cpus); i++) {
        cpu* c = list_get(lista_cpus, i);
        log_debug(kernel_log, "Dispatch: CPU %d - tipo=%d, pid=%d, fd=%d", 
                 c->id, c->tipo_conexion, c->pid, c->fd);
        if (c->tipo_conexion == CPU_DISPATCH && c->pid == -1) {
            cpu_disponible = c;
            log_info(kernel_log, "Dispatch: ✓ CPU %d seleccionada (fd=%d)", c->id, c->fd);
            break;
        }
    }
    
    if (!cpu_disponible) {
        pthread_mutex_unlock(&mutex_lista_cpus);
        log_error(kernel_log, "Dispatch: ✗ No hay CPUs disponibles para ejecutar PID %d", proceso_a_ejecutar->PID);
        // TODO: Manejar caso sin CPUs disponibles (encolar para después)
        return;
    }
    
    // Marcar la CPU como ocupada
    cpu_disponible->pid = proceso_a_ejecutar->PID;
    cpu_disponible->instruccion_actual = EXEC_OP;
    log_debug(kernel_log, "Dispatch: CPU %d marcada como ocupada con PID %d", 
             cpu_disponible->id, proceso_a_ejecutar->PID);
    pthread_mutex_unlock(&mutex_lista_cpus);
    
    // Enviar EXEC_OP al CPU con PID y PC
    log_debug(kernel_log, "Dispatch: Creando paquete EXEC_OP...");
    t_paquete* paquete = crear_paquete_op(EXEC_OP);
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PC);  // PC primero
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PID); // PID segundo
    
    log_info(kernel_log, "Dispatch: Enviando EXEC_OP a CPU %d (fd=%d) - PID: %d, PC: %d", 
             cpu_disponible->id, cpu_disponible->fd, proceso_a_ejecutar->PID, proceso_a_ejecutar->PC);
    
    enviar_paquete(paquete, cpu_disponible->fd);
    eliminar_paquete(paquete);
    
    log_info(kernel_log, "Dispatch: ✓ Proceso PID %d enviado a CPU %d", 
             proceso_a_ejecutar->PID, cpu_disponible->id);
}

void iniciar_planificador_corto_plazo(char* algoritmo){
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

void iniciar_planificador_largo_plazo() {
    log_trace(kernel_log, "Intentando iniciar planificador de largo plazo con algoritmo: %s", ALGORITMO_INGRESO_A_READY);

    if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0) {
        pthread_t manejoPlanificacionFIFO_lp;
        if (pthread_create(&manejoPlanificacionFIFO_lp, NULL, planificar_FIFO_lp, NULL) != 0) {
            log_error(kernel_log, "Error al crear hilo para planificador FIFO de largo plazo");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        pthread_detach(manejoPlanificacionFIFO_lp);
        log_trace(kernel_log, "Planificador FIFO de largo plazo iniciado correctamente");

    } else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0) {
        pthread_t manejoPlanificacionPMCP_lp;
        if (pthread_create(&manejoPlanificacionPMCP_lp, NULL, planificar_PMCP_lp, NULL) != 0) {
            log_error(kernel_log, "Error al crear hilo para planificador PMCP de largo plazo");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        pthread_detach(manejoPlanificacionPMCP_lp);
        log_trace(kernel_log, "Planificador PMCP de largo plazo iniciado correctamente");

    } else {
        log_error(kernel_log, "Algoritmo de planificacion de largo plazo no soportado (%s)", ALGORITMO_INGRESO_A_READY);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    pthread_t hilo_exit;
    if (pthread_create(&hilo_exit, NULL, gestionar_exit, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo para gestionar procesos en EXIT");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_exit);
    
    // CAMBIO: Iniciar planificador de corto plazo reactivo
    pthread_t hilo_planificador_cp;
    if (pthread_create(&hilo_planificador_cp, NULL, planificador_corto_plazo_reactivo, NULL) != 0) {
        log_error(kernel_log, "Error al crear hilo para planificador de corto plazo reactivo");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_planificador_cp);
    log_trace(kernel_log, "Planificador de corto plazo reactivo iniciado correctamente");
}

void* planificar_FIFO_lp(void* arg) {
    while (1) {
        // Esperar a que cola_new no este vacia (INIT_OP) 
        sem_wait(&sem_proceso_a_new);

        // Esperar a que cola_susp_ready este vacia
        sem_wait(&sem_susp_ready_vacia);

        // Elijo el primer proceso de la cola NEW
        pthread_mutex_lock(&mutex_cola_new);
        t_pcb* pcb = (t_pcb*)list_get(cola_new, 0);
        pthread_mutex_unlock(&mutex_cola_new);

        if (!pcb) {
            log_error(kernel_log, "planificar_FIFO_lp: No hay proceso en cola NEW pese a semáforo");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        log_trace(kernel_log, "planificar_FIFO_lp: Intentando inicializar PID %d", pcb->PID);

        // Solicitar memoria para el proceso elegido
        log_trace(kernel_log, "Enviando solicitud INIT_PROC_OP a Memoria para PID %d", pcb->PID);
        t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
        
        // Convertir enteros a strings para usar agregar_a_paquete consistentemente
        char* pid_str = string_itoa(pcb->PID);
        char* tamanio_str = string_itoa(pcb->tamanio_memoria);
        
        agregar_a_paquete(paquete, pid_str, strlen(pid_str) + 1);
        agregar_a_paquete(paquete, tamanio_str, strlen(tamanio_str) + 1);
        agregar_a_paquete(paquete, pcb->path, strlen(pcb->path) + 1);
        
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);
        
        // Liberar strings temporales
        free(pid_str);
        free(tamanio_str);
        log_trace(kernel_log, "FIFO-LP: Solicitud INIT_PROC_OP enviada a Memoria para PID %d", pcb->PID);

        // Esperar respuesta de Memoria
        t_respuesta_memoria respuesta;
        if (recv(fd_memoria, &respuesta, sizeof(t_respuesta_memoria), 0) <= 0) {
            log_error(kernel_log, "FIFO-LP: Error al recibir respuesta de Memoria para PID %d", pcb->PID);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        // Si la respuesta es positiva: transicionar a READY y seguir con el proximo
        if (respuesta == OK) {
            pthread_mutex_lock(&mutex_cola_new);
            list_remove_element(cola_new, pcb);
            pthread_mutex_unlock(&mutex_cola_new);

            cambiar_estado_pcb(pcb, READY);
            log_trace(kernel_log, "FIFO-LP: PID %d aceptado por Memoria y movido a READY", pcb->PID);
            // El planificador de corto plazo reactivo se activará automáticamente
        } else if (respuesta == ERROR) {    // Si la respuesta es negativa: se debera esperar al semaforo en EXIT que le avise que termino un proceso y reintentar
            log_trace(kernel_log, "FIFO-LP: Memoria rechazo PID %d, esperando liberacion de memoria", pcb->PID);

            // Esperar finalizacion de otro proceso
            sem_wait(&sem_finalizacion_de_proceso);
            
        } else {
            log_error(kernel_log, "FIFO-LP: error al intentar iniciar memoria para el proceso PID %d, mensaje de retorno invalido", pcb->PID);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }   
        sem_post(&sem_susp_ready_vacia);    
    }

    return NULL;
}

void* planificar_PMCP_lp(void* arg) {
    while (1) {
        // Esperar a que cola_new no este vacia (INIT_OP) 
        sem_wait(&sem_proceso_a_new);

        // Esperar a que cola_susp_ready este vacia
        sem_wait(&sem_susp_ready_vacia);

        // Elijo el proceso de la cola NEW
        t_pcb* pcb = elegir_por_pmcp();
        log_trace(kernel_log, "planificar_PMCP_lp: Intentando inicializar PID %d", pcb->PID);

        // Solicitar memoria para el proceso elegido
        log_trace(kernel_log, "Enviando solicitud INIT_PROC_OP a Memoria para PID %d", pcb->PID);
        t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
        
        // Convertir enteros a strings para usar agregar_a_paquete consistentemente
        char* pid_str = string_itoa(pcb->PID);
        char* tamanio_str = string_itoa(pcb->tamanio_memoria);
        
        agregar_a_paquete(paquete, pid_str, strlen(pid_str) + 1);
        agregar_a_paquete(paquete, tamanio_str, strlen(tamanio_str) + 1);
        agregar_a_paquete(paquete, pcb->path, strlen(pcb->path) + 1);
        
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);
        
        // Liberar strings temporales
        free(pid_str);
        free(tamanio_str);
        log_trace(kernel_log, "PMCP-LP: Solicitud INIT_PROC_OP enviada a Memoria para PID %d", pcb->PID);

        // Esperar respuesta de Memoria
        t_respuesta_memoria respuesta;
        if (recv(fd_memoria, &respuesta, sizeof(t_respuesta_memoria), 0) <= 0) {
            log_error(kernel_log, "PMCP-LP: Error al recibir respuesta de Memoria para PID %d", pcb->PID);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        // Si la respuesta es positiva: transicionar a READY y seguir con el proximo
        if (respuesta == OK) {
            pthread_mutex_lock(&mutex_cola_new);
            list_remove_element(cola_new, pcb);
            pthread_mutex_unlock(&mutex_cola_new);

            cambiar_estado_pcb(pcb, READY);
            log_trace(kernel_log, "PMCP-LP: PID %d aceptado por Memoria y movido a READY", pcb->PID);
            // El planificador de corto plazo reactivo se activará automáticamente
        } else if (respuesta == ERROR) {    // Si la respuesta es negativa: se debera esperar al semaforo en EXIT que le avise que termino un proceso o que entre un nuevo proceso a cola_new, y reintentar
            log_trace(kernel_log, "PMCP-LP: Memoria rechazo PID %d, esperando liberacion de memoria o entrada de nuevo proceso", pcb->PID);

            // Esperar finalizacion de otro proceso o liberacion de memoria
            // sem_wait(&sem_finalizacion_de_proceso);
            
        } else {
            log_error(kernel_log, "PMCP-LP: error al intentar iniciar memoria para el proceso PID %d, mensaje de retorno invalido", pcb->PID);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }       
        sem_post(&sem_susp_ready_vacia);
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

        log_debug(kernel_log, "gestionar_exit: Ejecutando syscall EXIT para PID=%d", pcb->PID);
        EXIT(pcb);
    }

    return NULL;
}

// NUEVO: Planificador de corto plazo reactivo
void* planificador_corto_plazo_reactivo(void* arg) {
    log_info(kernel_log, "=== PLANIFICADOR CP REACTIVO INICIADO ===");
    
    while (1) {
        log_debug(kernel_log, "Planificador CP reactivo: Esperando semáforo sem_proceso_a_ready...");
        
        // Esperar a que llegue un proceso a READY
        sem_wait(&sem_proceso_a_ready);
        
        log_info(kernel_log, "Planificador CP reactivo: ✓ Proceso llegó a READY - Iniciando evaluación");
        
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
        
        log_debug(kernel_log, "Planificador CP reactivo: CPUs totales=%d, CPUs dispatch=%d, CPUs libres=%d", 
                 cpus_totales, cpus_dispatch, cpus_libres);
        
        // Solo planificar si hay CPU disponible Y cola READY no vacía
        pthread_mutex_lock(&mutex_cola_ready);
        bool hay_procesos_ready = !list_is_empty(cola_ready);
        int procesos_en_ready = list_size(cola_ready);
        pthread_mutex_unlock(&mutex_cola_ready);
        
        log_debug(kernel_log, "Planificador CP reactivo: Procesos en READY=%d, hay_cpu_disponible=%s", 
                 procesos_en_ready, hay_cpu_disponible ? "SÍ" : "NO");
        
        if (hay_cpu_disponible && hay_procesos_ready) {
            log_info(kernel_log, "Planificador CP reactivo: ✓ Condiciones cumplidas - Iniciando planificación");
            iniciar_planificador_corto_plazo(ALGORITMO_CORTO_PLAZO);
            log_info(kernel_log, "Planificador CP reactivo: ✓ Planificación completada");
        } else {
            if (!hay_cpu_disponible) {
                log_warning(kernel_log, "Planificador CP reactivo: ⚠ No hay CPUs disponibles");
            }
            if (!hay_procesos_ready) {
                log_warning(kernel_log, "Planificador CP reactivo: ⚠ Cola READY vacía");
            }
            log_trace(kernel_log, "Planificador CP reactivo: Reposteando semáforo para reintento posterior");
            // Re-postear el semáforo para que se pueda reintentar cuando cambien las condiciones
            sem_post(&sem_proceso_a_ready);
        }
    }
    
    return NULL;
}
