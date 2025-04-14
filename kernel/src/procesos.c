#include "../headers/procesos.h"

/////////////////////////////// Funciones ///////////////////////////////
const char* estado_to_string(Estados estado) {
    switch (estado) {
        case NEW: return "NEW";
        case READY: return "READY";
        case EXEC: return "EXEC";
        case BLOCKED: return "BLOCKED";
        case SUSP_READY: return "SUSP_READY";
        case SUSP_BLOCKED: return "SUSP_BLOCKED";
        case EXIT_ESTADO: return "EXIT";
        default: return "ESTADO_DESCONOCIDO";
    }
}

void mostrar_pcb(t_pcb PCB) {
    printf("----- PCB -----\n");
    printf("PID: %u\n", PCB.PID);
    printf("PC: %u\n", PCB.PC);

    printf("ME: [");
    for (int i = 0; i < 7; i++) {
        printf("%u", PCB.ME[i]);
        if (i < 6) printf(", ");
    }
    printf("]\n");

    printf("MT: [");
    for (int i = 0; i < 7; i++) {
        printf("%u", PCB.MT[i]);
        if (i < 6) printf(", ");
    }
    printf("]\n");

    printf("Estado: %s\n", estado_to_string(PCB.Estado));
    printf("----------------\n");
}


// Pasa un PCB de una lista a otra
void cambiar_estado_pcb(t_pcb* PCB, t_list* estado_actual, t_list* nuevo_estado){
    if (nuevo_estado == cola_new) {
        PCB->Estado = NEW;
        list_add(cola_new, PCB);
    } else if (nuevo_estado == cola_ready) {
        PCB->Estado = READY;
        list_add(cola_ready, PCB);
    } else if (nuevo_estado == cola_running) {
        PCB->Estado = EXEC;
        list_add(cola_running, PCB);
    } else if (nuevo_estado == cola_blocked) {
        PCB->Estado = BLOCKED;
        list_add(cola_blocked, PCB);
    } else if (nuevo_estado == cola_susp_ready) {
        PCB->Estado = SUSP_READY;
        list_add(cola_susp_ready, PCB);
    } else if (nuevo_estado == cola_susp_blocked) {
        PCB->Estado = SUSP_BLOCKED;
        list_add(cola_susp_blocked, PCB);
    } else if (nuevo_estado == cola_exit) {
        PCB->Estado = EXIT_ESTADO;
        list_add(cola_exit, PCB);
    } else {
        log_error(kernel_log, "Error al mover el proceso de estados");
    }
    list_remove_element(estado_actual, PCB);
}