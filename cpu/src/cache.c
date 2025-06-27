#include "../headers/cache.h"
#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"

t_cache_paginas* inicializar_cache() {
    cache = (t_cache_paginas*)malloc(sizeof(t_cache_paginas));
    if (cache == NULL) {
        log_error(cpu_log,"No se pudo inicializar la cache");
        exit(EXIT_FAILURE);
    }
    cache->entradas = NULL;
    cache->cantidad_entradas = atoi(ENTRADAS_CACHE);
    if (cache_habilitada(cache)) {
        cache->entradas = (t_entrada_cache*)malloc(cache->cantidad_entradas * sizeof(t_entrada_cache));
        if (cache->entradas == NULL) {
            log_error(cpu_log, "Error al asignar memoria para las entradas de la cache");
            free(cache);
            exit(EXIT_FAILURE);
        }
        for (int i = 0 ; i < cache->cantidad_entradas ; i++) {
            cache->entradas[i].numero_pagina = -1;
            cache->entradas[i].contenido = NULL;
            cache->entradas[i].modificado = false;
            cache->entradas[i].bit_referencia = 0;
        }
    }
    cache->algoritmo_reemplazo = REEMPLAZO_CACHE;
    if (cache->algoritmo_reemplazo == NULL) {
        log_error(cpu_log, "Error al asignar memoria para las entradas de la cache");
        free(cache);
        exit(EXIT_FAILURE);
    }
    cache->puntero_clock = 0;
    return cache;
}
bool cache_habilitada() {
    return cache->cantidad_entradas > 0;
}
int buscar_pagina_en_cache ( int numero_pagina) {
    if (!cache_habilitada(cache))
        return -1;
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina == numero_pagina) {
            log_info(cpu_log, VERDE("PID: %d - Cache Hit - Pagina: %d"), pid_ejecutando, numero_pagina);
            if (strcmp(cache->algoritmo_reemplazo,"CLOCK") == 0 || strcmp(cache->algoritmo_reemplazo, "CLOCK-M") == 0) {
                cache->entradas[i].bit_referencia = 1; // actualizamos el bit de referencia
            }
            return i;
        }
    }
    return -1;
}

// funcion para seleccionar "victima" de reemplazo (clock)
int seleccionar_victima_clock() {
    while (1) {
        if (cache->entradas[cache->puntero_clock].bit_referencia == 0) {
            return cache->puntero_clock;
        }
        cache->entradas[cache->puntero_clock].bit_referencia = 0;
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    }
}

int seleccionar_victima_clock_m () {
    // busco los no modificados (Bit referencia = 0 & bit_modificado = 0)
    int comienzo = cache->puntero_clock;
    do {
        if (cache->entradas[cache->puntero_clock].bit_referencia == 0 && !cache->entradas[cache->puntero_clock].modificado) {
            return cache->puntero_clock;
        }
    }while (cache->puntero_clock != comienzo);

    // busco no modificados con bit referencia 1 (poner en 0) o con bit de referencia en 0
    do {
        if (cache->entradas[cache->puntero_clock].bit_referencia == 0) {
            return cache->puntero_clock;
        }
        cache->entradas[cache->puntero_clock].bit_referencia = 0;
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    } while (cache->puntero_clock != comienzo);
    return cache->puntero_clock;
}

char* acceder_a_pagina_en_cache(int numero_pagina) {
    if (cache == NULL)
        return NULL;
    if (!cache_habilitada(cache)) {
        log_trace(cpu_log, "La cache esta deshabilitada.");
        return NULL;
    }
    int nro_pagina_en_cache = buscar_pagina_en_cache(numero_pagina);
    if (nro_pagina_en_cache <= -1)
        return NULL;
    return cache->entradas[nro_pagina_en_cache].contenido;
}

void desalojar_proceso_cache() {
    if (!cache_habilitada(cache)) {
        log_error(cpu_log, "Cache deshabilitada");
        return;
    }

    for (int i = 0; i < cache->cantidad_entradas; i++) {
        log_trace(cpu_log, "Limpiando cache");

        if (cache->entradas[i].modificado && cache->entradas[i].numero_pagina >= 0) {
            int frameC = -1;
            int pagina = cache->entradas[i].numero_pagina;

            if (tlb_habilitada()) {
                bool encontrado = tlb_buscar(pagina, &frameC);
                if (!encontrado) {
                    log_warning(cpu_log, "TLB no contiene mapeo para página %d. No se pudo determinar frame.", pagina);
                }
            }

            log_info(cpu_log, VERDE("PID: %d - Memory Update - Página: %d - Frame: %d"), pid_ejecutando, pagina, frameC);

            // escribir_pagina_en_memoria(pagina, frame, cache->entradas[i].contenido);
        }

        free(cache->entradas[i].contenido);
        cache->entradas[i].contenido = NULL;
        cache->entradas[i].modificado = false;
        cache->entradas[i].bit_referencia = 0;
        cache->entradas[i].numero_pagina = -1;
    }

    cache->puntero_clock = 0;
}

void liberar_cache() {
    if (cache == NULL) {
        log_trace(cpu_log,"la cache ya estaba liberada.");
        return;
    }
    if (!cache_habilitada(cache)) {
        log_trace(cpu_log, "No hay entradas en la cache.");
        free(cache);
        cache = NULL;
        return;
    }
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].contenido != NULL) {
            free(cache->entradas[i].contenido);
        }
    }
    if (cache->entradas != NULL) {
        free(cache->entradas);
    }
    free(cache);
    cache = NULL;
}

void cache_modificar(int frame, char* datos) {
    if (cache == NULL) {
        log_trace(cpu_log,"la cache ya estaba liberada.");
        return;
    }
    if (!cache_habilitada(cache)) {
        log_trace(cpu_log, "La cache esta deshabilitada.");
        return;
    }
    int nro_pagina_en_cache = buscar_pagina_en_cache(frame);
    if (nro_pagina_en_cache <= -1) {
        log_trace(cpu_log, "No se encontro la pagina %d en la cache", frame);
        return;
    }
    cache->entradas[nro_pagina_en_cache].contenido = datos;
    cache->entradas[nro_pagina_en_cache].modificado = true;
}

void cache_escribir(int frame, char* datos) {
    if (cache == NULL) {
        log_trace(cpu_log,"la cache ya estaba liberada.");
        return;
    }
    if (!cache_habilitada(cache)) {
        log_trace(cpu_log, "La cache esta deshabilitada.");
        return;
    }
    
    // Buscar una entrada libre o seleccionar víctima
    int entrada_index = -1;
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina == -1) { // entrada libre
            entrada_index = i;
            break;
        }
    }
    
    if (entrada_index == -1) { // necesitamos reemplazar
        if (strcmp(cache->algoritmo_reemplazo, "CLOCK-M") == 0) {
            entrada_index = seleccionar_victima_clock_m();
        } else {
            entrada_index = seleccionar_victima_clock();
        }
    }
    
    // Limpiar entrada anterior si existe
    if (cache->entradas[entrada_index].contenido != NULL) {
        free(cache->entradas[entrada_index].contenido);
    }
    
    // Asignar nueva entrada
    cache->entradas[entrada_index].numero_pagina = frame;
    cache->entradas[entrada_index].contenido = datos;
    cache->entradas[entrada_index].modificado = false;
    cache->entradas[entrada_index].bit_referencia = 1;
    log_info(cpu_log, VERDE("PID: %d - Cache Add - Pagina: %d"), pid_ejecutando, frame);
}

char* cache_leer(int numero_pagina) {
    int indice = buscar_pagina_en_cache(numero_pagina);
    if (indice == -1) {
        log_warning(cpu_log, "PID: %d - Cache Leer - Página %d no encontrada en caché", pid_ejecutando, numero_pagina);
        return NULL;
    }

    if (cache->entradas[indice].contenido == NULL) {
        log_warning(cpu_log, "PID: %d - Cache Leer - Contenido nulo en entrada de página %d", pid_ejecutando, numero_pagina);
        return NULL;
    }

    // Devolvemos una copia del contenido para que el caller pueda hacer free
    char* copia = strdup(cache->entradas[indice].contenido);
    if (copia == NULL) {
        log_error(cpu_log, "PID: %d - Error al duplicar contenido de caché para página %d", pid_ejecutando, numero_pagina);
        return NULL;
    }
    return copia;
}