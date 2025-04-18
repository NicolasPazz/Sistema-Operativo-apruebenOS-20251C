#include "../headers/planificadores.h"


/////////////////////////////// Planificador Corto Plazo ///////////////////////////////
t_pcb* elegir_por_fifo(){
    log_debug(kernel_log, "PLANIFICANDO FIFO");

    // Se elegirá al siguiente proceso a ejecutar según su orden de llegada a READY.
    return (t_pcb*)list_get(cola_ready, 0);
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


}

t_pcb* elegir_por_srt(){
    log_debug(kernel_log, "PLANIFICANDO SRT");

    /*
        Funciona igual que el anterior con la variante que al ingresar un proceso en la cola de Ready
        existiendo al menos un proceso en Exec, se debe evaluar si dicho proceso tiene una rafaga más corta que 
        los que se encuentran en ejecución. En caso de ser así, se debe informar al CPU que posee al Proceso 
        con el tiempo más alto que debe desalojar al mismo para que pueda ser planificado el nuevo.
    */

}

void dispatch(t_pcb* proceso_a_ejecutar){

    // Una vez seleccionado el proceso a ejecutar, se lo transicionará al estado EXEC
    cambiar_estado_pcb(proceso_a_ejecutar, EXEC);

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
    }
    else {
        log_error(kernel_log, "iniciar_planificador_corto_plazo: Algoritmo no reconocido");
    }

    dispatch(proceso_elegido);
}