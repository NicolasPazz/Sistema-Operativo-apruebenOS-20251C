#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../headers/cache.h"
#include "../headers/cicloDeInstruccion.h"

t_cache_paginas* cache = NULL;
t_list* tlb = NULL;
int orden_fifo = 0;

void inicializar_mmu() {
    tlb = list_create();
    cache = inicializar_cache();    
}

bool recibir_config_memoria(int* tam_pagina, int* entradas_por_tabla, int* cantidad_niveles) {
    if (!enviar_operacion(fd_memoria, PEDIR_CONFIG_CPU_OP)) {
        return false;
    }
    return recibir_3_enteros(fd_memoria, tam_pagina, entradas_por_tabla, cantidad_niveles);
}

int traducir_direccion(int direccion_logica, int* desplazamiento) {
    int tam_pagina, entradas_por_tabla, cantidad_niveles;
    recibir_config_memoria(&tam_pagina, &entradas_por_tabla, &cantidad_niveles);

    int nro_pagina = direccion_logica / tam_pagina;
    // falta lo de entrada nivel x
    *desplazamiento = direccion_logica % tam_pagina;

    int entradas[cantidad_niveles];
    for (int nivel = 0; nivel < cantidad_niveles; nivel++) {
        int divisor = pow(entradas_por_tabla, cantidad_niveles - (nivel + 1));
        entradas[nivel] = (nro_pagina / divisor) % entradas_por_tabla;
    }

    int frame = 0;
    if (tlb_habilitada() && tlb_buscar(nro_pagina, &frame)) {
        log_info(cpu_log, "PID: %d - TLB HIT - Página: %d", pid_ejecutando, nro_pagina);    
    } else {
        log_info(cpu_log, "PID: %d - TLB MISS - Página: %d", pid_ejecutando, nro_pagina);

        // Enviar entradas de página a Memoria
            t_paquete* paquete = crear_paquete_op(SOLICITAR_FRAME_PARA_ENTRADAS);
            agregar_a_paquete(paquete, &pid_ejecutando, sizeof(int));
            agregar_a_paquete(paquete, &cantidad_niveles, sizeof(int));
            for (int i = 0; i < cantidad_niveles; i++) {
                agregar_a_paquete(paquete, &entradas[i], sizeof(int));
            }
            enviar_paquete(paquete, fd_memoria);
            eliminar_paquete(paquete);

        // Recibir frame
            recv(fd_memoria, &frame, sizeof(int), MSG_WAITALL);

        if (tlb_habilitada()) {
            tlb_insertar(nro_pagina, frame);
        }
    }

    return frame;
}

bool tlb_buscar(int pagina, int* frame_out) {
    for (int i = 0; i < list_size(tlb); i++) {
        entrada_tlb_t* entrada = list_get(tlb, i);
        if (entrada->valido && entrada->pagina == pagina) {
            *frame_out = entrada->frame;
            entrada->tiempo_uso = timestamp_actual(); // Para LRU: actualizamos uso
            return true;
        }
    }
    return false;
}

bool tlb_habilitada() {
    return atoi(ENTRADAS_TLB) > 0;
}

void tlb_insertar(int pagina, int frame) {
    entrada_tlb_t* nueva_entrada = malloc(sizeof(entrada_tlb_t));
    nueva_entrada->pagina = pagina;
    nueva_entrada->frame = frame;
    nueva_entrada->valido = true;
    nueva_entrada->tiempo_uso = timestamp_actual(); // Para LRU
    nueva_entrada->orden_fifo = orden_fifo++;        // Para FIFO

    if (list_size(tlb) < atoi(ENTRADAS_TLB)) {
        list_add(tlb, nueva_entrada);
    } else {
        int victima = seleccionar_victima_tlb();
        entrada_tlb_t* entrada_reemplazo = list_get(tlb, victima);
        free(entrada_reemplazo);
        list_replace(tlb, victima, nueva_entrada);
    }
}

int seleccionar_victima_tlb() {
    int victima = 0;
    if (strcmp(REEMPLAZO_TLB, "LRU") == 0) {
        int min_uso = UINT64_MAX;
        for (int i = 0; i < list_size(tlb); i++) {
            entrada_tlb_t* entrada = list_get(tlb, i);
            if (entrada->tiempo_uso < min_uso) {
                min_uso = entrada->tiempo_uso;
                victima = i;
            }
        }
    } else if (strcmp(REEMPLAZO_TLB, "FIFO") == 0) {
        int min_orden = UINT32_MAX;
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

int timestamp_actual() { // ACA CHATGPTIEE VER DE NUEVO ESTO...
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000); // en milisegundos
}