#include "../headers/organizador_eventos.h"
#include "../headers/kernel.h"
#include <unistd.h>

// Variables globales para coordinación de terminación
t_kernel_state kernel_state = KERNEL_RUNNING;
pthread_mutex_t mutex_kernel_state = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_finalizacion_completa;
volatile int hilos_activos = 0;
pthread_mutex_t mutex_hilos_activos = PTHREAD_MUTEX_INITIALIZER;

void inicializar_organizador_eventos(void) {
    kernel_state = KERNEL_RUNNING;
    hilos_activos = 0;
    
    if (sem_init(&sem_finalizacion_completa, 0, 0) != 0) {
        LOG_ERROR(kernel_log, "[ORGANIZADOR] Error al inicializar semáforo de finalización");
        exit(EXIT_FAILURE);
    }
    
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Organizador de eventos inicializado");
}

void destruir_organizador_eventos(void) {
    sem_destroy(&sem_finalizacion_completa);
    pthread_mutex_destroy(&mutex_kernel_state);
    pthread_mutex_destroy(&mutex_hilos_activos);
    
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Organizador de eventos destruido");
}

bool kernel_debe_continuar(void) {
    pthread_mutex_lock(&mutex_kernel_state);
    bool continuar = (kernel_state == KERNEL_RUNNING);
    pthread_mutex_unlock(&mutex_kernel_state);
    return continuar;
}

void iniciar_terminacion_kernel(void) {
    pthread_mutex_lock(&mutex_kernel_state);
    
    if (kernel_state != KERNEL_RUNNING) {
        pthread_mutex_unlock(&mutex_kernel_state);
        return; // Ya se inició la terminación
    }
    
    kernel_state = KERNEL_STOPPING;
    pthread_mutex_unlock(&mutex_kernel_state);
    
    log_info(kernel_log, "[ORGANIZADOR] Iniciando terminación ordenada del kernel");
    
    // PASO 1: Invalidar todos los timers de suspensión INMEDIATAMENTE
    invalidar_todos_timers_suspension();
    
    // PASO 2: Cancelar todas las operaciones de memoria pendientes
    cancelar_operaciones_memoria_pendientes();
    
    // PASO 3: Despertar todos los hilos bloqueados
    despertar_todos_hilos();
    
    // PASO 4: Dar tiempo a que los hilos procesen la señal de terminación
    usleep(100000); // 100ms para que los hilos vean el cambio de estado
    
    // PASO 5: Esperar a que todos los hilos terminen
    esperar_terminacion_todos_hilos();
    
    // Marcar como completamente detenido
    pthread_mutex_lock(&mutex_kernel_state);
    kernel_state = KERNEL_STOPPED;
    pthread_mutex_unlock(&mutex_kernel_state);
    
    log_info(kernel_log, "[ORGANIZADOR] Todos los hilos han terminado ordenadamente");
}

void registrar_hilo_activo(void) {
    pthread_mutex_lock(&mutex_hilos_activos);
    hilos_activos++;
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Hilo registrado. Hilos activos: %d", hilos_activos);
    pthread_mutex_unlock(&mutex_hilos_activos);
}

void registrar_hilo_terminado(void) {
    pthread_mutex_lock(&mutex_hilos_activos);
    hilos_activos--;
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Hilo terminado. Hilos activos restantes: %d", hilos_activos);
    
    if (hilos_activos == 0) {
        LOG_DEBUG(kernel_log, "[ORGANIZADOR] Todos los hilos han terminado");
        sem_post(&sem_finalizacion_completa);
    }
    pthread_mutex_unlock(&mutex_hilos_activos);
}

void esperar_terminacion_todos_hilos(void) {
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Esperando terminación de todos los hilos");
    
    // Verificar si ya no hay hilos activos
    pthread_mutex_lock(&mutex_hilos_activos);
    int count = hilos_activos;
    pthread_mutex_unlock(&mutex_hilos_activos);
    
    if (count == 0) {
        LOG_DEBUG(kernel_log, "[ORGANIZADOR] No hay hilos activos para esperar");
        return;
    }
    
    // Esperar la señal de finalización completa
    sem_wait(&sem_finalizacion_completa);
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Confirmada terminación de todos los hilos");
}

void despertar_todos_hilos(void) {
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Despertando todos los hilos bloqueados");
    
    // DESPERTAR AGRESIVAMENTE - enviar múltiples señales para asegurar que TODOS los hilos se despierten
    for (int i = 0; i < 25; i++) {  // Aumentado significativamente para máxima seguridad
        sem_post(&sem_interrupciones);
        sem_post(&sem_proceso_a_new);
        sem_post(&sem_proceso_a_exit);
        sem_post(&sem_planificador_cp);
        sem_post(&sem_procesos_rechazados);
        sem_post(&sem_proceso_a_susp_ready);
        sem_post(&sem_proceso_a_susp_blocked);
        sem_post(&sem_proceso_a_ready);
        sem_post(&sem_proceso_a_running);
        sem_post(&sem_proceso_a_blocked);
        sem_post(&sem_cpu_disponible);
        
        // Dar micro-pausa entre rondas para que los hilos procesen
        if (i % 5 == 0) usleep(10000); // 10ms cada 5 rondas
    }
    
    // Dar tiempo adicional para que los hilos procesen todas las señales
    usleep(200000); // 200ms para que todos los hilos reciban y procesen las señales
    
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] %d rondas de señales de despertar enviadas a todos los semáforos", 25);
}

t_kernel_state obtener_estado_kernel(void) {
    pthread_mutex_lock(&mutex_kernel_state);
    t_kernel_state estado = kernel_state;
    pthread_mutex_unlock(&mutex_kernel_state);
    return estado;
}

void invalidar_todos_timers_suspension(void) {
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Invalidando todos los timers de suspensión activos");
    
    // Necesitamos acceder a la lista de procesos para invalidar todos los timers
    extern t_list *cola_procesos;
    extern pthread_mutex_t mutex_cola_procesos;
    
    if (!cola_procesos) {
        LOG_DEBUG(kernel_log, "[ORGANIZADOR] No hay cola de procesos para invalidar timers");
        return;
    }
    
    LOCK_CON_LOG(mutex_cola_procesos);
    
    int timers_invalidados = 0;
    for (int i = 0; i < list_size(cola_procesos); i++) {
        t_pcb *pcb = list_get(cola_procesos, i);
        if (pcb && pcb->timer_flag) {
            LOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);
            if (pcb->timer_flag) {
                *(pcb->timer_flag) = false;  // Invalidar el timer
                pcb->timer_flag = NULL;      // Limpiar la referencia
                timers_invalidados++;
                LOG_DEBUG(kernel_log, "[ORGANIZADOR] Timer de suspensión invalidado para PID %d", pcb->PID);
            }
            UNLOCK_CON_LOG_PCB(pcb->mutex, pcb->PID);
        }
    }
    
    UNLOCK_CON_LOG(mutex_cola_procesos);
    
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] %d timers de suspensión invalidados", timers_invalidados);
}

void cancelar_operaciones_memoria_pendientes(void) {
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Cancelando operaciones de memoria pendientes");
    
    // Marcar flag global para que no se inicien nuevas operaciones de memoria
    // Las operaciones en curso fallarán graciosamente y se ignorarán los errores
    
    // No hay mucho que hacer aquí específicamente, pero el simple hecho de cambiar
    // el estado del kernel a STOPPING hace que conectar_memoria() retorne -1
    // y que los errores de enviar_op_memoria() se manejen mejor
    
    LOG_DEBUG(kernel_log, "[ORGANIZADOR] Operaciones de memoria marcadas para cancelación");
} 