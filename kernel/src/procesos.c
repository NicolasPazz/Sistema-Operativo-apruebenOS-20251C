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
    printf("-*-*-*-*-*- PCB -*-*-*-*-*-\n");
    printf("PID: %u\n", PCB.PID);
    printf("PC: %u\n", PCB.PC);
    mostrar_metrica("ME", PCB.ME);
    mostrar_metrica("MT", PCB.MT);
    printf("Estado: %s\n", estado_to_string(PCB.Estado));
    printf("-*-*-*-*-*-*-*-*-*-*-*-*-*-\n");
}

void mostrar_metrica(const char* nombre, uint16_t* metrica) {
    printf("%s: [", nombre);
    for (int i = 0; i < 7; i++) {
        printf("%u", metrica[i]);
        if (i < 6) printf(", ");
    }
    printf("]\n");
}

void cambiar_estado_pcb(t_pcb* PCB, Estados nuevo_estado_enum) {
    if (PCB == NULL) {
        log_error(kernel_log, "cambiar_estado_pcb: PCB es NULL");
        return;
    }

    if (!transicion_valida(PCB->Estado, nuevo_estado_enum)) {
        log_error(kernel_log, "cambiar_estado_pcb: Transicion no valida: %s → %s",
                  estado_to_string(PCB->Estado),
                  estado_to_string(nuevo_estado_enum));
        return;
    }

    t_list* cola_origen = obtener_cola_por_estado(PCB->Estado);
    t_list* cola_destino = obtener_cola_por_estado(nuevo_estado_enum);

    if (!cola_destino || !cola_origen) {
        log_error(kernel_log, "cambiar_estado_pcb: Error al obtener las colas correspondientes");
        return;
    }

    log_info(kernel_log, "Proceso %u cambio de estado: %s → %s",
             PCB->PID,
             estado_to_string(PCB->Estado),
             estado_to_string(nuevo_estado_enum));

    list_remove_element(cola_origen, PCB);
    PCB->Estado = nuevo_estado_enum;
    list_add(cola_destino, PCB);
}

bool transicion_valida(Estados actual, Estados destino) {
    switch (actual) {
        case NEW: return destino == READY;
        case READY: return destino == EXEC;
        case EXEC: return destino == BLOCKED || destino == READY;
        case BLOCKED: return destino == READY || destino == SUSP_BLOCKED;
        case SUSP_BLOCKED: return destino == SUSP_READY;
        case SUSP_READY: return destino == READY;
        default: return destino == EXIT_ESTADO;
    }
}

t_list* obtener_cola_por_estado(Estados estado) {
    switch (estado) {
        case NEW: return cola_new;
        case READY: return cola_ready;
        case EXEC: return cola_running;
        case BLOCKED: return cola_blocked;
        case SUSP_READY: return cola_susp_ready;
        case SUSP_BLOCKED: return cola_susp_blocked;
        case EXIT_ESTADO: return cola_exit;
        default: return NULL;
    }
}