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

    // Actualizar la ráfaga estimada para el SJF
    double rafaga_real = get_time() - pcb->tiempo_inicio_exec;
    double alfa = 0.5;
    pcb->estimacion_rafaga = alfa * rafaga_real + (1 - alfa) * pcb->estimacion_rafaga;

    // Cambiar a estado READY
    cambiar_estado_pcb(pcb, READY);
}