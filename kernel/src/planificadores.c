#include "../headers/planificadores.h"
#include <sys/time.h>

/////////////////////////////// Planificador Corto Plazo ///////////////////////////////
t_pcb* elegir_por_fifo(){
    log_debug(kernel_log, "PLANIFICANDO FIFO");

    // Se elegirá al siguiente proceso a ejecutar según su orden de llegada a READY.
    return (t_pcb*)list_get(cola_ready, 0);
}

void* menor_rafaga(void* a, void* b) {
    t_pcb* pcb_a = (t_pcb*) a;
    t_pcb* pcb_b = (t_pcb*) b;
    return pcb_a->estimacion_rafaga <= pcb_b->estimacion_rafaga ? pcb_a : pcb_b;
}
t_pcb* elegir_por_sjf(){
    log_debug(kernel_log, "PLANIFICANDO SJF");

    /*  Se elegirá el proceso que tenga la rafaga más corta.
        Su funcionamiento será como se explica en teoría y la función de cómo calcular las ráfagas es la siguiente
    
        Est(n) = Estimado de la ráfaga anterior
        R(n) = Lo que realmente ejecutó de la ráfaga anterior en la CPU

        Est(n+1) = El estimado de la próxima ráfaga
        Est(n+1) =  R(n) + (1-) Est(n) ;     [0,1]
    */

    return (t_pcb*)list_get_minimum(cola_ready, menor_rafaga); // Elige al PCB con la menor ESTIMACIÓN de ráfaga
}

t_pcb* elegir_por_srt(){
    log_debug(kernel_log, "PLANIFICANDO SRT");

    /*
        Funciona igual que el anterior con la variante que al ingresar un proceso en la cola de Ready
        existiendo al menos un proceso en Exec, se debe evaluar si dicho proceso tiene una rafaga más corta que 
        los que se encuentran en ejecución. En caso de ser así, se debe informar al CPU que posee al Proceso 
        con el tiempo más alto que debe desalojar al mismo para que pueda ser planificado el nuevo.
    

    pthread_t hilo_algoritmo_srt;
    pthread_create(&hilo_algoritmo_srt, NULL, chequear_ready, NULL);
    pthread_detach(hilo_algoritmo_srt);
    */
    //t_pcb* menor_rafaga = list_get_minimum(cola_ready, menor_rafaga);

    return (t_pcb*)list_get(cola_ready, 0);
}

void dispatch(t_pcb* proceso_a_ejecutar){

    // Una vez seleccionado el proceso a ejecutar, se lo transicionará al estado EXEC
    cambiar_estado_pcb(proceso_a_ejecutar, EXEC);
    proceso_a_ejecutar->tiempo_inicio_exec = get_time();

    // Enviar a CPU el PID del proceso a ejecutar
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
        log_error(kernel_log, "iniciar_planificador_corto_plazo: Cola READY vacía");
        return;
    }
    else {
        log_error(kernel_log, "iniciar_planificador_corto_plazo: Algoritmo no reconocido");
        return;
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
    log_info(kernel_log, "Intentando iniciar planificador de largo plazo con algoritmo: %s", ALGORITMO_INGRESO_A_READY);

    if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0) {
        pthread_t manejoPlanificacionFIFO_lp;
        if (pthread_create(&manejoPlanificacionFIFO_lp, NULL, planificar_FIFO_lp, NULL) != 0) {
            log_error(kernel_log, "Error al crear hilo para planificador FIFO de largo plazo");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        pthread_detach(manejoPlanificacionFIFO_lp);
        log_info(kernel_log, "Planificador FIFO de largo plazo iniciado correctamente");

    } else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0) {
        pthread_t manejoPlanificacionPMCP_lp;
        if (pthread_create(&manejoPlanificacionPMCP_lp, NULL, planificar_PMCP_lp, NULL) != 0) {
            log_error(kernel_log, "Error al crear hilo para planificador PMCP de largo plazo");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        pthread_detach(manejoPlanificacionPMCP_lp);
        log_info(kernel_log, "Planificador PMCP de largo plazo iniciado correctamente");

    } else {
        log_error(kernel_log, "Algoritmo de planificación de largo plazo no soportado (%s)", ALGORITMO_INGRESO_A_READY);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
}

void* planificar_FIFO_lp() {
    while (1) {
        // Esperar a que cola_new no este vacia (INIT_OP) 
        pthread_mutex_lock(&mutex_cola_new);
        while (list_is_empty(cola_new)) {
            log_trace(kernel_log, "planificar_FIFO_lp: Esperando procesos en cola NEW...");
            pthread_cond_wait(&cond_nuevo_proceso, &mutex_cola_new);
        }
        pthread_mutex_unlock(&mutex_cola_new);

        // Esperar a que cola_susp_ready este vacia
        pthread_mutex_lock(&mutex_cola_susp_ready);
        while (!list_is_empty(cola_susp_ready)) {
            log_trace(kernel_log, "planificar_FIFO_lp: Esperando que SUSP_READY esté vacía...");
            pthread_cond_wait(&cond_susp_ready_vacia, &mutex_cola_susp_ready);
        }
        pthread_mutex_unlock(&mutex_cola_susp_ready);

        // Elijo el primer proceso de la cola NEW
        pthread_mutex_lock(&mutex_cola_new);
        t_pcb* pcb = (t_pcb*)list_get(cola_new, 0);
        pthread_mutex_unlock(&mutex_cola_new);
        log_info(kernel_log, "planificar_FIFO_lp: Intentando inicializar PID %d", pcb->PID);

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
            continue;
        }
        // Si la respuesta es positiva: transicionar a READY y seguir con el proximo
        if (respuesta == OK) {
            pthread_mutex_lock(&mutex_cola_new);
            list_remove_element(cola_new, pcb);
            pthread_mutex_unlock(&mutex_cola_new);

            cambiar_estado_pcb(pcb, READY);
            log_info(kernel_log, "FIFO-LP: PID %d aceptado por Memoria y movido a READY", pcb->PID);
            // Notificar al planificador de corto o mediano plazo dependiendo la transicion
        } else if (respuesta == ERROR) {    // Si la respuesta es negativa: se debera esperar al semaforo en EXIT que le avise que termino un proceso y reintentar
            log_warning(kernel_log, "FIFO-LP: Memoria rechazó PID %d, esperando liberación de memoria", pcb->PID);

            // Esperar señal de memoria liberada
            pthread_mutex_lock(&mutex_cola_exit);
            pthread_cond_wait(&cond_exit, &mutex_cola_exit);
            pthread_mutex_unlock(&mutex_cola_exit);
        } else {
            log_error(kernel_log, "FIFO-LP: error al intentar iniciar memoria para el proceso PID %d, mensaje de retorno invalido", pcb->PID);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }       
    }

    return NULL;
}

void* planificar_PMCP_lp() {
    while (1) {
        // Esperar a que cola_new no este vacia (INIT_OP) 
        pthread_mutex_lock(&mutex_cola_new);
        while (list_is_empty(cola_new)) {
            log_trace(kernel_log, "planificar_PMCP_lp: Esperando procesos en cola NEW...");
            pthread_cond_wait(&cond_nuevo_proceso, &mutex_cola_new);
        }
        pthread_mutex_unlock(&mutex_cola_new);

        // Esperar a que cola_susp_ready este vacia
        pthread_mutex_lock(&mutex_cola_susp_ready);
        while (!list_is_empty(cola_susp_ready)) {
            log_trace(kernel_log, "planificar_PMCP_lp: Esperando que SUSP_READY esté vacía...");
            pthread_cond_wait(&cond_susp_ready_vacia, &mutex_cola_susp_ready);
        }
        pthread_mutex_unlock(&mutex_cola_susp_ready);

        // Elijo el proceso de la cola NEW
        t_pcb* pcb = elegir_por_pmcp();
        log_info(kernel_log, "planificar_PMCP_lp: Intentando inicializar PID %d", pcb->PID);

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
            continue;
        }
        // Si la respuesta es positiva: transicionar a READY y seguir con el proximo
        if (respuesta == OK) {
            pthread_mutex_lock(&mutex_cola_new);
            list_remove_element(cola_new, pcb);
            pthread_mutex_unlock(&mutex_cola_new);

            cambiar_estado_pcb(pcb, READY);
            log_info(kernel_log, "PMCP-LP: PID %d aceptado por Memoria y movido a READY", pcb->PID);
            // Notificar al planificador de corto o mediano plazo dependiendo la transicion
        } else if (respuesta == ERROR) {    // Si la respuesta es negativa: se debera esperar al semaforo en EXIT que le avise que termino un proceso o que entre un nuevo proceso a cola_new, y reintentar
            log_warning(kernel_log, "PMCP-LP: Memoria rechazó PID %d, esperando liberación de memoria o entrada de nuevo proceso", pcb->PID);

            // Esperar señal de memoria liberada o proceso nuevo
            pthread_mutex_lock(&mutex_replanificar_pmcp);
            pthread_cond_wait(&cond_replanificar_pmcp, &mutex_replanificar_pmcp);
            pthread_mutex_unlock(&mutex_replanificar_pmcp);
        } else {
            log_error(kernel_log, "PMCP-LP: error al intentar iniciar memoria para el proceso PID %d, mensaje de retorno invalido", pcb->PID);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }       
    }

    return NULL;
}

void* menor_tamanio(void* a, void* b) {
    t_pcb* pcb_a = (t_pcb*) a;
    t_pcb* pcb_b = (t_pcb*) b;
    return pcb_a->tamanio_memoria <= pcb_b->tamanio_memoria ? pcb_a : pcb_b;
}

t_pcb* elegir_por_pmcp() {
    log_debug(kernel_log, "PLANIFICANDO PMCP (Proceso Más Chico Primero)");
    pthread_mutex_lock(&mutex_cola_new);
    t_pcb* pcb_mas_chico = (t_pcb*)list_get_minimum(cola_new, menor_tamanio);
    pthread_mutex_unlock(&mutex_cola_new);
    return (t_pcb*)pcb_mas_chico;
}