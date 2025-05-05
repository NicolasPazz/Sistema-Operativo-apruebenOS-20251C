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

    // Una vez seleccionado el proceso a ejecutar, se lo transicionara al estado EXEC
    cambiar_estado_pcb(proceso_a_ejecutar, EXEC);
    proceso_a_ejecutar->tiempo_inicio_exec = get_time();

    // Enviar a CPU el PID del proceso a ejecutar y en lista_cpus relacionar el pid del proceso enviado, con la cpu a la que se manda a ejecutar
    log_trace(kernel_log, "Enviando PID %d a CPU por Dispatch para que lo ejecute", proceso_a_ejecutar->PID);
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

void fin_io(t_pcb* pcb){

    // Cambiar a estado READY
    cambiar_estado_pcb(pcb, READY);
    log_info(kernel_log, "## (<%d>) finalizó IO y pasa a READY", pcb->PID);
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
        agregar_entero_a_paquete(paquete, pcb->PID);
        agregar_entero_a_paquete(paquete, pcb->tamanio_memoria);
        agregar_a_paquete(paquete, pcb->path, strlen(pcb->path) + 1);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);
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
            // Notificar al planificador de corto o mediano plazo dependiendo la transicion
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
        agregar_entero_a_paquete(paquete, pcb->PID);
        agregar_entero_a_paquete(paquete, pcb->tamanio_memoria);
        agregar_a_paquete(paquete, pcb->path, strlen(pcb->path) + 1);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);
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
            // Notificar al planificador de corto o mediano plazo dependiendo la transicion?
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
