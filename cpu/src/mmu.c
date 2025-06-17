#define _POSIX_C_SOURCE 199309L
#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../headers/cache.h"
#include "../headers/cicloDeInstruccion.h"
#include "../../memoria/headers/init_memoria.h"
#include <time.h>

t_cache_paginas* cache = NULL;
t_list* tlb = NULL;
int orden_fifo = 0;
t_config_memoria* cfg_memoria = NULL;

void inicializar_mmu() {
    tlb = list_create();
    cache = inicializar_cache();    
}

int cargar_configuracion(char* path) {
    t_config* cfg_file = config_create(path);

    if (cfg_file == NULL) {
        log_error(cpu_log, "No se encontro el archivo de configuracion: %s", path);
        return 0;
    }

    char* properties[] = {
        "PUERTO_ESCUCHA",
        "TAM_MEMORIA",
        "TAM_PAGINA",
        "ENTRADAS_POR_TABLA",
        "CANTIDAD_NIVELES",
        "RETARDO_MEMORIA",
        "PATH_SWAPFILE",
        "RETARDO_SWAP",
        "LOG_LEVEL",
        "DUMP_PATH",
        NULL
    };

    if (!config_has_all_properties(cfg_file, properties)) {
        log_error(cpu_log, "Propiedades faltantes en el archivo de configuracion");
        config_destroy(cfg_file);
        return 0;
    }

    cfg_memoria = malloc(sizeof(t_config_memoria));
    if (cfg_memoria == NULL) {
        log_error(cpu_log, "Error al asignar memoria para la configuracion");
        config_destroy(cfg_file);
        return 0;
    }

    cfg_memoria->PUERTO_ESCUCHA = config_get_int_value(cfg_file, "PUERTO_ESCUCHA");
    cfg_memoria->TAM_MEMORIA = config_get_int_value(cfg_file, "TAM_MEMORIA");
    cfg_memoria->TAM_PAGINA = config_get_int_value(cfg_file, "TAM_PAGINA");
    cfg_memoria->ENTRADAS_POR_TABLA = config_get_int_value(cfg_file, "ENTRADAS_POR_TABLA");
    cfg_memoria->CANTIDAD_NIVELES = config_get_int_value(cfg_file, "CANTIDAD_NIVELES");
    cfg_memoria->RETARDO_MEMORIA = config_get_int_value(cfg_file, "RETARDO_MEMORIA");
    cfg_memoria->PATH_SWAPFILE = strdup(config_get_string_value(cfg_file, "PATH_SWAPFILE"));
    cfg_memoria->RETARDO_SWAP = config_get_int_value(cfg_file, "RETARDO_SWAP");
    cfg_memoria->LOG_LEVEL = strdup(config_get_string_value(cfg_file, "LOG_LEVEL"));
    cfg_memoria->DUMP_PATH = strdup(config_get_string_value(cfg_file, "DUMP_PATH"));

    log_trace(cpu_log, "Archivo de configuracion cargado correctamente");
    config_destroy(cfg_file);

    return 1;
}

int traducir_direccion(int direccion_logica, int* desplazamiento) {

    int tam_pagina = cfg_memoria->TAM_PAGINA;
    int entradas_por_tabla = cfg_memoria->ENTRADAS_POR_TABLA;
    int cantidad_niveles = cfg_memoria->CANTIDAD_NIVELES;

    
    int nro_pagina = direccion_logica / tam_pagina;
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
        log_info(cpu_log, "PID: %d - OBTENER MARCO - Página: %d - Marco: %d", pid_ejecutando, nro_pagina, frame);

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