#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include <commons/log.h>
#include <commons/string.h>
#include <stdio.h>
#include <time.h>

// Variables externas
extern t_sistema_memoria* sistema_memoria;
extern t_log* logger;

// ============== FUNCIONES DE CREACIÓN DE MÉTRICAS ==============

t_metricas_proceso* crear_metricas_proceso(int pid) {
    t_metricas_proceso* metricas = malloc(sizeof(t_metricas_proceso));
    if (!metricas) {
        log_error(logger, "Error al crear métricas para proceso %d", pid);
        return NULL;
    }

    metricas->pid = pid;
    metricas->accesos_tabla_paginas = 0;
    metricas->instrucciones_solicitadas = 0;
    metricas->bajadas_swap = 0;
    metricas->subidas_memoria_principal = 0;
    metricas->lecturas_memoria = 0;
    metricas->escrituras_memoria = 0;
    metricas->timestamp_creacion = time(NULL);
    metricas->timestamp_ultimo_acceso = time(NULL);

    pthread_mutex_init(&metricas->mutex_metricas, NULL);

    return metricas;
}

// ============== FUNCIONES DE INCREMENTO DE MÉTRICAS ==============

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
    
    // Formato formal según consigna
    log_debug(logger, VERDE("## PID: %d - Proceso Destruido - Métricas - Acc.T.Pag: %d; Inst.Sol.: %d; SWAP: %d; Mem.Prin.: %d; Lec.Mem.: %d; Esc.Mem.: %d"), 
             pid, 
             metricas->accesos_tabla_paginas,
             metricas->instrucciones_solicitadas,
             metricas->bajadas_swap,
             metricas->subidas_memoria_principal,
             metricas->lecturas_memoria,
             metricas->escrituras_memoria);
    
    // Formato legible (mantener para debugging)
    log_info(logger, VERDE("## PID: %d - Proceso Destruido - Métricas finales:"), pid);
    log_info(logger, "    "AZUL("Acc.T.Pag:")"      "VERDE("%d"), metricas->accesos_tabla_paginas);
    log_info(logger, "    "AZUL("Inst.Sol.:")"      "VERDE("%d"), metricas->instrucciones_solicitadas);
    log_info(logger, "    "AZUL("SWAP:")"           "VERDE("%d"), metricas->bajadas_swap);
    log_info(logger, "    "AZUL("Mem.Prin.:")"      "VERDE("%d"), metricas->subidas_memoria_principal);
    log_info(logger, "    "AZUL("Lec.Mem.:")"       "VERDE("%d"), metricas->lecturas_memoria);
    log_info(logger, "    "AZUL("Esc.Mem.:")"       "VERDE("%d"), metricas->escrituras_memoria);
    
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