#define _POSIX_C_SOURCE 199309L
#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../headers/cache.h"
#include "../headers/cicloDeInstruccion.h"
#include "../../memoria/headers/init_memoria.h"
#include <time.h>
#include "../headers/main.h"

t_cache_paginas* cache = NULL;
t_list* tlb = NULL;
int orden_fifo = 0;
t_config_memoria* cfg_memoria = NULL;

void inicializar_mmu() {
    tlb = list_create();
    cache = inicializar_cache();    
}

int traducir_direccion_fisica(int direccion_logica) {
    if (cfg_memoria == NULL) {
        log_error(cpu_log, "ERROR: cfg_memoria no inicializada");
        exit(EXIT_FAILURE);
    }

    int tam_pagina = cfg_memoria->TAM_PAGINA;
    

    int nro_pagina = direccion_logica / tam_pagina;
    int desplazamiento = direccion_logica % tam_pagina;

    int frame = 0;
    bool hit = false;

    if (tlb_habilitada()) {
        pthread_mutex_lock(&mutex_tlb);
        hit = tlb_buscar(pid_ejecutando, nro_pagina, &frame);
        pthread_mutex_unlock(&mutex_tlb);

        if (hit) {
            log_info(cpu_log, "PID: %d - TLB HIT - Página: %d", pid_ejecutando, nro_pagina);
        } else {
            log_info(cpu_log, "PID: %d - TLB MISS - Página: %d", pid_ejecutando, nro_pagina);
        }
    }

    if (!hit) {
        t_paquete* paquete = crear_paquete_op(ACCESO_TABLA_PAGINAS_OP);
        agregar_entero_a_paquete(paquete, pid_ejecutando);
        agregar_entero_a_paquete(paquete, nro_pagina);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

        op_code codigo_operacion;
        if (recv(fd_memoria, &codigo_operacion, sizeof(op_code), MSG_WAITALL) != sizeof(op_code)) {
            log_error(cpu_log, "PID: %d - Error al recibir op_code de respuesta desde Memoria", pid_ejecutando);
            exit(EXIT_FAILURE);
        }

        if (codigo_operacion != PAQUETE_OP) {
            log_error(cpu_log, "PID: %d - Op_code inesperado en respuesta: %d (esperaba PAQUETE_OP)", pid_ejecutando, codigo_operacion);
            exit(EXIT_FAILURE);
        }

        t_list* lista_respuesta = recibir_contenido_paquete(fd_memoria);
        if (lista_respuesta == NULL || list_size(lista_respuesta) < 1) {
            log_error(cpu_log, "PID: %d - Error al recibir respuesta de marco desde Memoria", pid_ejecutando);
            exit(EXIT_FAILURE);
        }

        frame = *(int*)list_get(lista_respuesta, 0);
        list_destroy_and_destroy_elements(lista_respuesta, free);

        if (frame == -1) {
            log_error(cpu_log, "PID: %d - Error al traducir dirección: Marco no encontrado", pid_ejecutando);
            exit(EXIT_FAILURE);
        }

        log_info(cpu_log, "PID: %d - OBTENER MARCO - Página: %d - Marco: %d", pid_ejecutando, nro_pagina, frame);

        if (tlb_habilitada()) {
            pthread_mutex_lock(&mutex_tlb);
            tlb_insertar(pid_ejecutando, nro_pagina, frame);
            pthread_mutex_unlock(&mutex_tlb);
            log_trace(cpu_log, "TLB insertada (PID=%d): Página %d -> Frame %d", pid_ejecutando, nro_pagina, frame);
        }
    }

    return frame * tam_pagina + desplazamiento;
}


bool tlb_buscar(int pid, int pagina, int* frame_out) {
    for (int i = 0; i < list_size(tlb); i++) {
        entrada_tlb_t* entrada = list_get(tlb, i);
        if (entrada->valido && entrada->pagina == pagina && entrada->pid == pid) {
            *frame_out = entrada->frame;
            entrada->tiempo_uso = timestamp_actual();
            return true;
        }
    }
    return false;
}

bool tlb_habilitada() {
    return atoi(ENTRADAS_TLB) > 0;
}

void tlb_insertar(int pid, int pagina, int frame) {
    entrada_tlb_t* nueva_entrada = malloc(sizeof(entrada_tlb_t));
    nueva_entrada->pid = pid;
    nueva_entrada->pagina = pagina;
    nueva_entrada->frame = frame;
    nueva_entrada->valido = true;
    nueva_entrada->tiempo_uso = timestamp_actual();
    nueva_entrada->orden_fifo = orden_fifo++;

    if (list_size(tlb) < atoi(ENTRADAS_TLB)) {
        // log_info(cpu_log, "ENTRO A agregar la pagina");
        list_add(tlb, nueva_entrada);
    } else {
        // log_info(cpu_log, "ENTRO A REEMPLAZAR LA VICTIMA");
        int victima = seleccionar_victima_tlb();
        entrada_tlb_t* entrada_reemplazo = list_get(tlb, victima);

        // 🔽 Logueamos el reemplazo antes de liberar la entrada
        log_info(cpu_log,
            PURPURA("Reemplazo en TLB: Página=%d reemplazado por Página=%d"),
            entrada_reemplazo->pagina, nueva_entrada->pagina);

        free(entrada_reemplazo);
        list_replace(tlb, victima, nueva_entrada);
    }
}


int seleccionar_victima_tlb() {
    int victima = 0;
    if (strcmp(REEMPLAZO_TLB, "LRU") == 0) {
        int min_uso = INT_MAX;
        for (int i = 0; i < list_size(tlb); i++) {
            entrada_tlb_t* entrada = list_get(tlb, i);
            if (entrada->tiempo_uso < min_uso) {
                min_uso = entrada->tiempo_uso;
                victima = i;
            }
        }
    } else if (strcmp(REEMPLAZO_TLB, "FIFO") == 0) {
        int min_orden = INT_MAX;
        for (int i = 0; i < list_size(tlb); i++) {
            entrada_tlb_t* entrada = list_get(tlb, i);
            if (entrada->orden_fifo < min_orden) {
                min_orden = entrada->orden_fifo;
                victima = i;
            }
        }
    }
    return victima;
}

long timestamp_actual() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void desalojar_proceso_tlb(int pid) {
    pthread_mutex_lock(&mutex_tlb);
    if (!tlb_habilitada()) {
        pthread_mutex_unlock(&mutex_tlb);
        log_trace(cpu_log, "TLB deshabilitada");
        return;
    }

    log_trace(cpu_log, "Limpiando TLB para proceso %d", pid);

    for (int i = 0; i < list_size(tlb); i++) {
        entrada_tlb_t* entrada = list_get(tlb, i);
        if (entrada && entrada->pid == pid) {
            log_trace(cpu_log, "Limpiando entrada TLB: PID %d, Página %d, Frame %d", entrada->pid, entrada->pagina, entrada->frame);
            free(entrada);
            list_remove(tlb, i);
            i--; // ajustar índice tras eliminación
        }
    }

    log_trace(cpu_log, "TLB limpiada exitosamente para proceso %d", pid);
    pthread_mutex_unlock(&mutex_tlb);
}