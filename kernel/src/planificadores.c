#include "../headers/planificadores.h"


/////////////////////////////// Planificador Corto Plazo ///////////////////////////////
t_pcb* planificar_por_fifo(){
    log_debug(kernel_log, "PLANIFICANDO FIFO");
}
void planificar_por_sjf(){
    log_debug(kernel_log, "PLANIFICANDO SJF");
}
void planificar_por_srt(){
    log_debug(kernel_log, "PLANIFICANDO SRT");
}

void iniciar_planificador_corto_plazo(char* algoritmo){
    if (strcmp(algoritmo, "FIFO") == 0) {
        planificar_por_fifo();
    } else if (strcmp(algoritmo, "SJF") == 0) {
        planificar_por_sjf();
    } else if (strcmp(algoritmo, "SRT") == 0) {
        planificar_por_srt();
    } else {
        log_error(kernel_log, "iniciar_planificador_corto_plazo: Algoritmo no reconocido");
    }
}