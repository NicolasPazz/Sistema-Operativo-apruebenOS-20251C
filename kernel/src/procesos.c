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
    printf("Tiempo inicio exec: %f\n", PCB.tiempo_inicio_exec);
    printf("Rafaga estimada: %.2f\n", PCB.estimacion_rafaga);
    printf("Path: %s\n", PCB.path);
    printf("Tamanio de memoria: %u\n", PCB.tamanio_memoria);
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

void mostrar_colas_estados() {
    printf("Colas -> [NEW: %d, READY: %d, EXEC: %d, BLOCK: %d, SUSP.BLOCK: %d, SUSP.READY: %d, EXIT: %d] | Procesos en total: %d\n",
        list_size(cola_new),
        list_size(cola_ready),
        list_size(cola_running),
        list_size(cola_blocked),
        list_size(cola_susp_blocked),
        list_size(cola_susp_ready),
        list_size(cola_exit),
        list_size(cola_procesos));
}

void cambiar_estado_pcb(t_pcb* PCB, Estados nuevo_estado_enum) {
    if (PCB == NULL) {
        log_trace(kernel_log, "cambiar_estado_pcb: PCB es NULL");
        return;
    }

    if (!transicion_valida(PCB->Estado, nuevo_estado_enum)) {
        log_trace(kernel_log, "cambiar_estado_pcb: Transicion no valida: %s → %s",
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

    log_info(kernel_log, "## (<%u>) Pasa del estado <%s> al estado <%s>",
             PCB->PID,
             estado_to_string(PCB->Estado),
             estado_to_string(nuevo_estado_enum));

    list_remove_element(cola_origen, PCB);

    // Actualizar Metricas de Tiempo antes de cambiar de Estado
    char* pid_key = string_itoa(PCB->PID);
    t_temporal* cronometro = dictionary_get(tiempos_por_pid, pid_key); 
    if (cronometro != NULL) {
        temporal_stop(cronometro);
        int64_t tiempo = temporal_gettime(cronometro); // 10 seg

        // Guardar el tiempo en el estado ANTERIOR
        PCB->MT[PCB->Estado] += (uint16_t)tiempo;
        log_trace(kernel_log, "Se actualizo el MT en el estado %s del PID %d con %ld", estado_to_string(PCB->Estado), PCB->PID, tiempo);
        temporal_destroy(cronometro);

        // Reiniciar el cronometro para el nuevo estado
        cronometro = temporal_create();
        dictionary_put(tiempos_por_pid, pid_key, cronometro);
    }
    free(pid_key);

    // Si pasa al Estado EXEC hay que actualizar el tiempo_inicio_exec
    if(nuevo_estado_enum == EXEC){
        PCB->tiempo_inicio_exec = get_time();
    } else if (PCB->Estado == EXEC && nuevo_estado_enum == BLOCKED){
        // Cuando SALE de EXEC calculo la estimacion proxima
        double rafaga_real = get_time() - PCB->tiempo_inicio_exec;
        double alfa = 0.5;
        PCB->estimacion_rafaga = alfa * rafaga_real + (1 - alfa) * PCB->estimacion_rafaga;

    }

    // Cambiar Estado y actualizar Metricas de Estados
    PCB->Estado = nuevo_estado_enum;
    PCB->ME[nuevo_estado_enum] += 1;  // Se suma 1 en las Metricas de estado del nuevo estado

    list_add(cola_destino, PCB);
}

bool transicion_valida(Estados actual, Estados destino) {
    switch (actual) {
        case NEW: return destino == READY;
        case READY: return destino == EXEC;
        case EXEC: return destino == BLOCKED || destino == READY || destino == EXIT_ESTADO;
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


/*

    4. Cuando el PCB termina (EXIT), destruis su cronometro y lo quitas del diccionario:
    c
    Copiar
    Editar
    char* pid_key = string_itoa(PCB->PID);
    dictionary_remove_and_destroy(tiempos_por_pid, pid_key, (void*)temporal_destroy);
    free(pid_key);



    5. Cuando el sistema termina, limpias todo:
    c
    Copiar
    Editar
    dictionary_destroy_and_destroy_elements(tiempos_por_pid, (void*)temporal_destroy);

*/