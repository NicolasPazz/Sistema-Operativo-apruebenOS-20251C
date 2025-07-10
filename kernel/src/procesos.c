#include "../headers/procesos.h"
#include <commons/collections/list.h>

/////////////////////////////// Funciones ///////////////////////////////
const char* estado_to_string(Estados estado) {
    switch (estado) {
        case INIT: return "INIT";
        case NEW: return "NEW";
        case READY: return "READY";
        case EXEC: return "EXEC";
        case BLOCKED: return "BLOCKED";
        case SUSP_READY: return "SUSP_READY";
        case SUSP_BLOCKED: return "SUSP_BLOCKED";
        case EXIT_ESTADO: return "EXIT";
        default: log_error(kernel_log, "estado_to_string: Estado desconocido %d", estado);
                 terminar_kernel();
                 exit(EXIT_FAILURE);
    }
}

void mostrar_pcb(t_pcb* PCB) {
    if (PCB == NULL) {
        log_error(kernel_log, "mostrar_pcb: PCB es NULL");
        return;
    }

    log_trace(kernel_log, "-*-*-*-*-*- PCB -*-*-*-*-*-");
    log_trace(kernel_log, "PID: %d", PCB->PID);
    log_trace(kernel_log, "PC: %d", PCB->PC);
    mostrar_metrica("ME", PCB->ME);
    mostrar_metrica("MT", PCB->MT);
    log_trace(kernel_log, "Estado: %s", estado_to_string(PCB->Estado));
    log_trace(kernel_log, "Tiempo inicio exec: %.3f", PCB->tiempo_inicio_exec);
    log_trace(kernel_log, "Rafaga estimada: %.2f", PCB->estimacion_rafaga);
    log_trace(kernel_log, "Path: %s", PCB->path ? PCB->path : "(null)");
    log_trace(kernel_log, "Tamanio de memoria: %d", PCB->tamanio_memoria);
    log_trace(kernel_log, "-*-*-*-*-*-*-*-*-*-*-*-*-*-");
}

void mostrar_metrica(const char* nombre, int* metrica) {
    char buffer[256];
    int offset = snprintf(buffer, sizeof(buffer), "%s: [", nombre);

    for (int i = 0; i < 7; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%u", metrica[i]);
        if (i < 6) offset += snprintf(buffer + offset, sizeof(buffer) - offset, ", ");
    }

    snprintf(buffer + offset, sizeof(buffer) - offset, "]");

    log_trace(kernel_log, "%s", buffer);
}

void mostrar_colas_estados() {
    log_trace(kernel_log, "Colas -> [NEW: %d, READY: %d, EXEC: %d, BLOCK: %d, SUSP.BLOCK: %d, SUSP.READY: %d, RECHAZADOS: %d, EXIT: %d] | Procesos en total: %d",
        list_size(cola_new),
        list_size(cola_ready),
        list_size(cola_running),
        list_size(cola_blocked),
        list_size(cola_susp_blocked),
        list_size(cola_susp_ready),
        list_size(cola_rechazados),
        list_size(cola_exit),
        list_size(cola_procesos));
}

void cambiar_estado_pcb(t_pcb* PCB, Estados nuevo_estado_enum) {
    log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] INICIO - Estado actual: %s, Nuevo estado: %s", 
              PCB->PID, estado_to_string(PCB->Estado), estado_to_string(nuevo_estado_enum));
    
    if (PCB == NULL) {
        log_error(kernel_log, "cambiar_estado_pcb: PCB es NULL");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    if (!transicion_valida(PCB->Estado, nuevo_estado_enum)) {
        log_error(kernel_log, "cambiar_estado_pcb: Transicion no valida en el PID %d: %s → %s",
                  PCB->PID,
                  estado_to_string(PCB->Estado),
                  estado_to_string(nuevo_estado_enum));
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Transición válida confirmada", PCB->PID);
    
    t_list* cola_destino = obtener_cola_por_estado(nuevo_estado_enum);
    if (!cola_destino) {
        log_error(kernel_log, "cambiar_estado_pcb: Error al obtener las colas correspondientes");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Cola destino obtenida correctamente", PCB->PID);

    if (PCB->Estado != INIT) {
        t_list* cola_origen = obtener_cola_por_estado(PCB->Estado);

        if (!cola_origen) {
            log_error(kernel_log, "cambiar_estado_pcb: Error al obtener las colas correspondientes");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        log_info(kernel_log, AZUL("## (%u) Pasa del estado ")VERDE("%s")AZUL(" al estado ")VERDE("%s"),
                PCB->PID,
                estado_to_string(PCB->Estado),
                estado_to_string(nuevo_estado_enum));
        
        bloquear_cola_por_estado(PCB->Estado);
        list_remove_element(cola_origen, PCB);
        liberar_cola_por_estado(PCB->Estado);
        
        // Asegurar que el proceso esté en cola_procesos
        bool ya_en_cola_procesos = false;
        pthread_mutex_lock(&mutex_cola_procesos);
        for (int i = 0; i < list_size(cola_procesos); i++) {
            t_pcb* pcb_existente = list_get(cola_procesos, i);
            if (pcb_existente->PID == PCB->PID) {
                ya_en_cola_procesos = true;
                break;
            }
        }
        if (!ya_en_cola_procesos) {
            list_add(cola_procesos, PCB);
            log_trace(kernel_log, "cambiar_estado_pcb: PID %d agregado a cola_procesos", PCB->PID);
        }
        pthread_mutex_unlock(&mutex_cola_procesos);

        // Actualizar Metricas de Tiempo antes de cambiar de Estado
        char* pid_key = string_itoa(PCB->PID);
        t_temporal* cronometro = dictionary_get(tiempos_por_pid, pid_key); 
        if (cronometro != NULL) {
            temporal_stop(cronometro);
            int64_t tiempo = temporal_gettime(cronometro); // 10 seg

            // Guardar el tiempo en el estado ANTERIOR
            PCB->MT[PCB->Estado] += (int)tiempo;
            log_trace(kernel_log, "Se actualizo el MT en el estado %s del PID %d con %ld", estado_to_string(PCB->Estado), PCB->PID, tiempo);
            temporal_destroy(cronometro);

            // Reiniciar el cronometro para el nuevo estado
            cronometro = temporal_create();
            dictionary_put(tiempos_por_pid, pid_key, cronometro);
        }
        free(pid_key);

        // Si pasa al Estado EXEC hay que actualizar el tiempo_inicio_exec
        if (nuevo_estado_enum == EXEC) {
            PCB->tiempo_inicio_exec = get_time();
        } else if (PCB->Estado == EXEC) {
            // calculo la estimacion proxima
            double rafaga_real = get_time() - PCB->tiempo_inicio_exec;
            PCB->estimacion_rafaga = ALFA * rafaga_real + (1 - ALFA) * PCB->estimacion_rafaga;
            // reiniciar el tiempo de inicio
            PCB->tiempo_inicio_exec = -1;
        }

        if (nuevo_estado_enum == BLOCKED) {
            PCB->tiempo_inicio_blocked = get_time();
            iniciar_timer_suspension(PCB);
        } else if (PCB->Estado == BLOCKED) {
            // reiniciar el tiempo de inicio
            PCB->tiempo_inicio_blocked = -1;
            //invalidar el timer vigente
            if (PCB->timer_flag) {
                *(PCB->timer_flag) = false;
                PCB->timer_flag = NULL;
            }
        }

        if (PCB->Estado == SUSP_READY) {
            sem_post(&sem_susp_ready_vacia); // Sumar 1 al semaforo
            log_debug(kernel_log, "cambiar_estado_pcb: Semaforo SUSP READY VACIA aumentado");
        }

        // Cambiar Estado y actualizar Metricas de Estados
        PCB->Estado = nuevo_estado_enum;
        PCB->ME[nuevo_estado_enum] += 1;  // Se suma 1 en las Metricas de estado del nuevo estado
    } else {       
        log_trace(kernel_log, "cambiar_estado_pcb: proceso en INIT recibido");
        char* pid_key = string_itoa(PCB->PID);
        if (!dictionary_get(tiempos_por_pid, pid_key)) {
            t_temporal* nuevo_crono = temporal_create();
            dictionary_put(tiempos_por_pid, pid_key, nuevo_crono);
        }
        free(pid_key);

        // Cambiar Estado y actualizar Metricas de Estados
        PCB->Estado = nuevo_estado_enum;
        PCB->ME[nuevo_estado_enum] += 1;  // Se suma 1 en las Metricas de estado del nuevo estado
        
        // Agregar a cola_procesos
        pthread_mutex_lock(&mutex_cola_procesos);
        list_add(cola_procesos, PCB);
        log_trace(kernel_log, "cambiar_estado_pcb: PID %d agregado a cola_procesos (INIT)", PCB->PID);
        pthread_mutex_unlock(&mutex_cola_procesos);
        
        bloquear_cola_por_estado(PCB->Estado);
        list_add(cola_destino, PCB);
        liberar_cola_por_estado(PCB->Estado);
    }

    bloquear_cola_por_estado(nuevo_estado_enum);
    list_add(cola_destino, PCB);
    liberar_cola_por_estado(nuevo_estado_enum);

    switch(nuevo_estado_enum) {
        case NEW: 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Antes de sem_post(&sem_proceso_a_new)", PCB->PID);
            sem_post(&sem_proceso_a_new); 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Semaforo a NEW aumentado", PCB->PID); 
            break;
        case READY: 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Antes de sem_post(&sem_proceso_a_ready)", PCB->PID);
            sem_post(&sem_proceso_a_ready); 
            solicitar_replanificacion_srt(); 
            log_trace(kernel_log, "cambiar_estado_pcb: replanificacion solicitada"); 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Semaforo a READY aumentado", PCB->PID); 
            break;
        case EXEC: 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Antes de sem_post(&sem_proceso_a_running)", PCB->PID);
            sem_post(&sem_proceso_a_running); 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Semaforo a EXEC aumentado", PCB->PID); 
            break;
        case BLOCKED: 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Antes de sem_post(&sem_proceso_a_blocked)", PCB->PID);
            sem_post(&sem_proceso_a_blocked); 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Semaforo a BLOCKED aumentado", PCB->PID); 
            break;
        case SUSP_READY:    log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] INICIO caso SUSP_READY", PCB->PID);
                            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Antes de sem_post(&sem_proceso_a_susp_ready)", PCB->PID);
                            sem_post(&sem_proceso_a_susp_ready);
                            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Semaforo a SUSP READY aumentado", PCB->PID);
                            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] FIN caso SUSP_READY", PCB->PID);
                            break;
        case SUSP_BLOCKED: 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Antes de sem_post(&sem_proceso_a_susp_blocked)", PCB->PID);
            sem_post(&sem_proceso_a_susp_blocked); 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Semaforo a SUSP BLOCKED aumentado", PCB->PID); 
            break;
        case EXIT_ESTADO: 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Antes de loguear_metricas_estado", PCB->PID);
            loguear_metricas_estado(PCB); 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Antes de sem_post(&sem_proceso_a_exit)", PCB->PID);
            sem_post(&sem_proceso_a_exit); 
            log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Semaforo a EXIT aumentado", PCB->PID); 
            break;
        default: log_error(kernel_log, "nuevo_estado_enum: Error al pasar PCB a %s", estado_to_string(nuevo_estado_enum));
                 terminar_kernel();
                 exit(EXIT_FAILURE);
    }

    // Lógica para el timer de suspensión del planificador mediano plazo
    if (nuevo_estado_enum == BLOCKED) {
        log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Iniciando planificador mediano plazo", PCB->PID);
        planificador_mediano_plazo(PCB);
    } else if (PCB->Estado == BLOCKED && nuevo_estado_enum != SUSP_BLOCKED) {
        log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Cancelando timer de suspensión", PCB->PID);
        PCB->cancelar_timer_suspension = true;
    }

    log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] Antes de mostrar_colas_estados()", PCB->PID);
    mostrar_colas_estados();
    log_debug(kernel_log, "cambiar_estado_pcb: [PID %d] FIN - Función completada exitosamente", PCB->PID);
}

bool transicion_valida(Estados actual, Estados destino) {
    switch (actual) {
        case INIT: return destino == NEW;
        case NEW: return destino == READY;
        case READY: return destino == EXEC;
        case EXEC: return destino == BLOCKED || destino == READY || destino == EXIT_ESTADO;
        case BLOCKED: return destino == READY || destino == SUSP_BLOCKED || destino == EXIT_ESTADO;
        case SUSP_BLOCKED: return destino == SUSP_READY;
        case SUSP_READY: return destino == READY;
        default: log_error(kernel_log, "transicion_valida: Estado desconocido %d", actual);
                 terminar_kernel();
                 exit(EXIT_FAILURE);
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
        default: log_error(kernel_log, "obtener_cola_por_estado: Estado desconocido %d", estado);
                 terminar_kernel();
                 exit(EXIT_FAILURE);
    }
}

void bloquear_cola_por_estado(Estados estado) {
    switch (estado) {
        case NEW:
            log_debug(kernel_log, "bloquear_cola_por_estado: esperando mutex_cola_new para bloquear cola NEW");
            pthread_mutex_lock(&mutex_cola_new);
            log_debug(kernel_log, "bloquear_cola_por_estado: bloqueando mutex_cola_new para cola NEW");
            break;
        case READY:
            log_debug(kernel_log, "bloquear_cola_por_estado: esperando mutex_cola_ready para bloquear cola READY");
            pthread_mutex_lock(&mutex_cola_ready);
            log_debug(kernel_log, "bloquear_cola_por_estado: bloqueando mutex_cola_ready para cola READY");
            break;
        case EXEC:
            log_debug(kernel_log, "bloquear_cola_por_estado: esperando mutex_cola_running para bloquear cola EXEC");
            pthread_mutex_lock(&mutex_cola_running);
            log_debug(kernel_log, "bloquear_cola_por_estado: bloqueando mutex_cola_running para cola EXEC");
            break;
        case BLOCKED:
            log_debug(kernel_log, "bloquear_cola_por_estado: esperando mutex_cola_blocked para bloquear cola BLOCKED");
            pthread_mutex_lock(&mutex_cola_blocked);
            log_debug(kernel_log, "bloquear_cola_por_estado: bloqueando mutex_cola_blocked para cola BLOCKED");
            break;
        case SUSP_READY:
            log_debug(kernel_log, "bloquear_cola_por_estado: esperando mutex_cola_susp_ready para bloquear cola SUSP_READY");
            pthread_mutex_lock(&mutex_cola_susp_ready);
            log_debug(kernel_log, "bloquear_cola_por_estado: bloqueando mutex_cola_susp_ready para cola SUSP_READY");
            break;
        case SUSP_BLOCKED:
            log_debug(kernel_log, "bloquear_cola_por_estado: esperando mutex_cola_susp_blocked para bloquear cola SUSP_BLOCKED");
            pthread_mutex_lock(&mutex_cola_susp_blocked);
            log_debug(kernel_log, "bloquear_cola_por_estado: bloqueando mutex_cola_susp_blocked para cola SUSP_BLOCKED");
            break;
        case EXIT_ESTADO:
            log_debug(kernel_log, "bloquear_cola_por_estado: esperando mutex_cola_exit para bloquear cola EXIT");
            pthread_mutex_lock(&mutex_cola_exit);
            log_debug(kernel_log, "bloquear_cola_por_estado: bloqueando mutex_cola_exit para cola EXIT");
            break;
        default:
            log_error(kernel_log, "bloquear_cola_por_estado: Estado desconocido %d", estado);
            terminar_kernel();
            exit(EXIT_FAILURE);
    }
}

void liberar_cola_por_estado(Estados estado) {
    switch (estado) {
        case NEW: pthread_mutex_unlock(&mutex_cola_new); break;
        case READY: pthread_mutex_unlock(&mutex_cola_ready); break;
        case EXEC: pthread_mutex_unlock(&mutex_cola_running); break;
        case BLOCKED: pthread_mutex_unlock(&mutex_cola_blocked); break;
        case SUSP_READY: pthread_mutex_unlock(&mutex_cola_susp_ready); break;
        case SUSP_BLOCKED: pthread_mutex_unlock(&mutex_cola_susp_blocked); break;
        case EXIT_ESTADO: pthread_mutex_unlock(&mutex_cola_exit); break;
        default: log_error(kernel_log, "liberar_cola_por_estado: Estado desconocido %d", estado);
                 terminar_kernel();
                 exit(EXIT_FAILURE);
    }
}

void loguear_metricas_estado(t_pcb* pcb) {
    if (!pcb) return;

    log_info(kernel_log, NARANJA("## (%d) - Métricas de estado:"), pcb->PID);

    for (int i = 0; i < 7; i++) {
        const char* nombre_estado = estado_to_string((Estados)i);
        unsigned veces = pcb->ME[i];
        unsigned tiempo = pcb->MT[i];
        log_info(kernel_log, "    "NARANJA("%-12s")" Veces: "VERDE("%-2u")" | Tiempo: "VERDE("%-6u ms"), nombre_estado, veces, tiempo);
    }
}

t_pcb* buscar_pcb(int pid) {
    log_debug(kernel_log, "buscar_pcb: esperando mutex_cola_procesos para buscar PCB del proceso %d", pid);
    pthread_mutex_lock(&mutex_cola_procesos);
    log_debug(kernel_log, "buscar_pcb: bloqueando mutex_cola_procesos para buscar PCB del proceso %d", pid);

    t_pcb* resultado = NULL;

    for (int i = 0; i < list_size(cola_procesos); i++) {
        t_pcb* pcb = list_get(cola_procesos, i);
        if (pcb->PID == pid) {
            resultado = pcb;
            break;
        }
    }

    pthread_mutex_unlock(&mutex_cola_procesos);

    if (!resultado) {
        log_error(kernel_log, "buscar_pcb: No se encontró PCB para PID=%d", pid);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    return resultado;
}

t_pcb* buscar_y_remover_pcb_por_pid(t_list* cola, int pid) {
    if (!cola) {
        log_error(kernel_log, "buscar_y_remover_pcb_por_pid: cola es NULL");
        return NULL;
    }

    // Buscar y remover el PCB de la cola específica
    for (int i = 0; i < list_size(cola); i++) {
        t_pcb* pcb = list_get(cola, i);
        if (pcb && pcb->PID == pid) {
            return list_remove(cola, i);
        }
    }

    // No se encontró el PCB
    log_warning(kernel_log, "buscar_y_remover_pcb_por_pid: No se encontró PCB con PID %d en la cola", pid);
    return NULL;
}
