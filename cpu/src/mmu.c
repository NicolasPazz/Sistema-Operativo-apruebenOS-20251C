#include "../headers/mmu.h"
#include "../../utils/headers/sockets.h"
#include "../headers/init.h"
#include "../headers/cache.h"
#include "../headers/cicloDeInstruccion.h"

t_cache_paginas* cache = NULL;
t_list* tlb = NULL;
uint32_t orden_fifo = 0;

void inicializar_mmu() {
    tlb = list_create();
    // Inicializar cache de paginas (Fijarse en funciones.c como hacer para solo inicializar una vez la cache...)
    cache = inicializar_cache();    // warning: assignment to ‘t_list *’ from incompatible pointer type ‘t_cache_paginas *’
}

uint32_t traducir_direccion(uint32_t direccion_logica, uint32_t* desplazamiento) {
    t_paquete* paquete = crear_paquete_op(PEDIR_CONFIG_CPU_OP); // VER TEMA CASE EN MEMORIA PARA QUE ME MANDEN LAS 4 CONFIG
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    int cod_op = recibir_operacion(fd_memoria);

    t_list* config_memoria = recibir_4_enteros(fd_memoria);
    uint32_t tam_memoria = (uint32_t)(uintptr_t)list_get(config_memoria, 0);
    uint32_t tam_pagina = (uint32_t)(uintptr_t)list_get(config_memoria, 1);
    uint32_t entradas_por_tabla = (uint32_t)(uintptr_t)list_get(config_memoria, 2);
    uint32_t cantidad_niveles = (uint32_t)(uintptr_t)list_get(config_memoria, 3);

    uint32_t nro_pagina = direccion_logica / tam_pagina;
    // falta lo de entrada nivel x
    *desplazamiento = direccion_logica % tam_pagina;

    uint32_t frame = 0;
    if (tlb_habilitada() && tlb_buscar(nro_pagina, &frame)) {
        log_info(cpu_log, "PID: %d - TLB HIT - Página: %d", pid_ejecutando, nro_pagina);    
    } else {
        log_info(cpu_log, "PID: %d - TLB MISS - Página: %d", pid_ejecutando, nro_pagina);

        frame =5; // ??
        //solicitar_frame_memoria(nro_pagina);  VER PEDIR POR PAQUETE PEDIR FRAM MEMORIA COMO OP CODE Y AGREGAR AL PAQUETE EL FRAME QUE NECESITO
        if (tlb_habilitada()) {
            tlb_insertar(nro_pagina, frame);
        }
    }
    return frame;
}
bool tlb_buscar(uint32_t pagina, uint32_t* frame_out) {
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
    return ENTRADAS_TLB > 0;
}

void tlb_insertar(uint32_t pagina, uint32_t frame) {
    entrada_tlb_t* nueva_entrada = malloc(sizeof(entrada_tlb_t));
    nueva_entrada->pagina = pagina;
    nueva_entrada->frame = frame;
    nueva_entrada->valido = true;
    nueva_entrada->tiempo_uso = timestamp_actual(); // Para LRU
    nueva_entrada->orden_fifo = orden_fifo++;        // Para FIFO

    if (list_size(tlb) < ENTRADAS_TLB) {    // warning: comparison between pointer and integer
        list_add(tlb, nueva_entrada);
    } else {
        // TLB llena
        int victima = seleccionar_victima_tlb();
        entrada_tlb_t* entrada_reemplazo = list_get(tlb, victima);
        free(entrada_reemplazo);
        list_replace(tlb, victima, nueva_entrada);
    }
}

int seleccionar_victima_tlb() {
    int victima = 0;
    if (strcmp(REEMPLAZO_TLB, "LRU") == 0) {
        uint64_t min_uso = UINT64_MAX;
        for (int i = 0; i < list_size(tlb); i++) {
            entrada_tlb_t* entrada = list_get(tlb, i);
            if (entrada->tiempo_uso < min_uso) {
                min_uso = entrada->tiempo_uso;
                victima = i;
            }
        }
    } else if (strcmp(REEMPLAZO_TLB, "FIFO") == 0) {
        uint32_t min_orden = UINT32_MAX;
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

uint64_t timestamp_actual() { // ACA CHATGPTIEE VER DE NUEVO ESTO...
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000); // en milisegundos
}