#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include <commons/log.h>
#include <commons/string.h>
#include <stdio.h>

// Variables externas
extern t_sistema_memoria* sistema_memoria;
extern t_log* logger;

void incrementar_accesos_tabla_paginas(int pid) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_str);
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->accesos_tabla_paginas++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
}

void incrementar_instrucciones_solicitadas(int pid) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_str);
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->instrucciones_solicitadas++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
}

void incrementar_bajadas_swap(int pid) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_str);
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->bajadas_swap++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
}

void incrementar_subidas_memoria_principal(int pid) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_str);
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->subidas_memoria_principal++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
}

void incrementar_lecturas_memoria(int pid) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_str);
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->lecturas_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
}

void incrementar_escrituras_memoria(int pid) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_str);
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->escrituras_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
}

t_metricas_proceso* obtener_metricas_proceso(int pid) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    return dictionary_get(sistema_memoria->metricas_procesos, pid_str);
}

void imprimir_metricas_proceso(int pid) {
    t_metricas_proceso* metricas = obtener_metricas_proceso(pid);
    if (metricas == NULL) {
        log_warning(logger, "PID: %d - Métricas no encontradas", pid);
        return;
    }
    
    pthread_mutex_lock(&metricas->mutex_metricas);
    
    // Formato EXACTO según consigna:
    // "Acc.T.Pag: <ATP>; Inst.Sol.: <Inst.Sol.>; SWAP: <SWAP>; Mem.Prin.: <Mem.Prin.>; Lec.Mem.: <Lec.Mem.>; Esc.Mem.: <Esc.Mem.>"
    log_info(logger, "## PID: %d - Proceso Destruido - Métricas - Acc.T.Pag: %d; Inst.Sol.: %d; SWAP: %d; Mem.Prin.: %d; Lec.Mem.: %d; Esc.Mem.: %d",
             pid,
             metricas->accesos_tabla_paginas,
             metricas->instrucciones_solicitadas,
             metricas->bajadas_swap,
             metricas->subidas_memoria_principal,
             metricas->lecturas_memoria,
             metricas->escrituras_memoria);
    
    pthread_mutex_unlock(&metricas->mutex_metricas);
}

void actualizar_metricas(int pid, char* operacion) {
    if (operacion == NULL) {
        log_warning(logger, "PID: %d - Operación nula para actualizar métricas", pid);
        return;
    }
    
    // Actualizar métricas según el tipo de operación
    if (strcmp(operacion, "MEMORY_READ") == 0) {
        incrementar_lecturas_memoria(pid);
    } else if (strcmp(operacion, "MEMORY_WRITE") == 0) {
        incrementar_escrituras_memoria(pid);
    } else if (strcmp(operacion, "TABLE_ACCESS") == 0) {
        incrementar_accesos_tabla_paginas(pid);
    } else if (strcmp(operacion, "INSTRUCTION_REQUEST") == 0) {
        incrementar_instrucciones_solicitadas(pid);
    } else if (strcmp(operacion, "SWAP_OUT") == 0) {
        incrementar_bajadas_swap(pid);
    } else if (strcmp(operacion, "SWAP_IN") == 0) {
        incrementar_subidas_memoria_principal(pid);
    } else {
        log_warning(logger, "PID: %d - Operación desconocida para métricas: %s", pid, operacion);
    }
} 