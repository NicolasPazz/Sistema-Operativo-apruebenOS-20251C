#include "../headers/cache.h"
#include "../headers/mmu.h"
#include "../headers/init.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

t_cache_paginas* inicializar_cache(){
    t_cache_paginas* cache = (t_cache_paginas*)malloc(sizeof(t_cache_paginas));
    if (cache == NULL){
        log_error(cpu_log,"No se pudo inicializar la cache");
        exit(EXIT_FAILURE);
    }
    cache->entradas = NULL;
    if (ENTRADAS_CACHE > 0) {
        cache->entradas = (t_entrada_cache*)malloc(atoi(ENTRADAS_CACHE) * sizeof(t_entrada_cache));
        if(cache->entradas == NULL){
            log_error(cpu_log, "Error al asignar memoria para las entradas de la cache");
            free(cache);
            exit(EXIT_FAILURE);
        }
        for (int i = 0 ; i < ENTRADAS_CACHE ; i++){
            cache->entradas[i].numero_pagina = -1;
            cache->entradas[i].contenido = NULL;
            cache->entradas[i].modificado = false;
            cache->entradas[i].bit_referencia = 0;
        }
    }
    cache->algoritmo_reemplazo = REEMPLAZO_CACHE;
    if(cache->algoritmo_reemplazo == NULL){
        log_error(cpu_log, "Error al asignar memoria para las entradas de la cache");
        free(cache);
        exit(EXIT_FAILURE);
    }
    cache->puntero_clock = 0;
    return cache;
}

int buscar_pagina_en_cache (t_cache_paginas* cache, int numero_pagina){
    if (ENTRADAS_CACHE == 0)
        return -1;
    for(int i = 0; i < ENTRADAS_CACHE; i++){
        if (cache->entradas[i].numero_pagina == numero_pagina){
            if(strcmp(cache->algoritmo_reemplazo,"CLOCK") == 0 || strcmp(cache->algoritmo_reemplazo, "CLOCK-M") == 0){
                cache->entradas[i].bit_referencia = 1; // actualizamos el bit de referencia
            }
            return i;
        }
    }
    return -1;
}

// funcion para seleccionar "victima" de reemplazo (clock)
int seleccionar_victima_clock(t_cache_paginas* cache){
    while (1){
        if(cache->entradas[cache->puntero_clock].bit_referencia == 0){
            return cache->puntero_clock;
        }
        cache->entradas[cache->puntero_clock].bit_referencia = 0;
        cache->puntero_clock = (cache->puntero_clock + 1) % atoi(ENTRADAS_CACHE);
    }
}

int seleccionar_victima_clock_m (t_cache_paginas* cache){
    // busco los no modificados (Bit referencia = 0 & bit_modificado = 0)
    int comienzo = cache->puntero_clock;
    do {
        if(cache->entradas[cache->puntero_clock].bit_referencia == 0 && !cache->entradas[cache->puntero_clock].modificado){
            return cache->puntero_clock;
        }
    }while(cache->puntero_clock != comienzo);

    // busco no modificados con bit referencia 1 (poner en 0) o con bit de referencia en 0
    do {
        if(cache->entradas[cache->puntero_clock].bit_referencia == 0){
            return cache->puntero_clock;
        }
        cache->entradas[cache->puntero_clock].bit_referencia = 0;
        cache->puntero_clock = (cache->puntero_clock + 1) % atoi(ENTRADAS_CACHE);
    } while(cache->puntero_clock != comienzo);
    return cache->puntero_clock;
}

char* acceder_a_pagina_en_cache(t_cache_paginas* cache, int numero_pagina){
    if(cache == NULL)
        return NULL;
    if (ENTRADAS_CACHE < 1){
        log_info(cpu_log, "La cache esta deshabilitada.");
        return NULL;
    }
    int nro_pagina_en_cache = buscar_pagina_en_cache(cache,numero_pagina);
    if (nro_pagina_en_cache <= -1)
        return NULL;
    return cache->entradas[nro_pagina_en_cache].contenido;
}

void desalojar_proceso_cache(t_cache_paginas* cache){ 
    if (ENTRADAS_CACHE <= 0){
        log_error(cpu_log, "Cache deshabilitada");
        EXIT_FAILURE;
    }
    for (int i = 0; i < ENTRADAS_CACHE; i++){
        log_info(cpu_log, "Limpiando cache");
        if (cache->entradas[i].modificado && !(cache->entradas[i].numero_pagina <= -1)){
            log_info(cpu_log, "Actualizando pagina modificada %d en memoria", cache->entradas[i].numero_pagina);
            escribir_pagina_en_memoria();

        }
        free(cache->entradas[i].contenido);
        cache->entradas[i].contenido = NULL;
        cache->entradas[i].modificado = false;
        cache->entradas[i].bit_referencia = 0;
        cache->entradas[i].numero_pagina = -1;
    }
    cache->puntero_clock = 0; // reseteo el puntero del reloj
}

void liberar_cache(t_cache_paginas* cache){
    if (cache == NULL){
        log_info(cpu_log,"la cache ya estaba liberada.");
        EXIT_SUCCESS;
    }
    if(ENTRADAS_CACHE <= 0){
        log_info(cpu_log, "No hay entradas en la cache.");
        EXIT_SUCCESS;
    }
    for (int i = 0; i<ENTRADAS_CACHE; i++){
        if (cache->entradas != NULL){
            for (int i = 0; i < ENTRADAS_CACHE; i++)
            {
                free(cache->entradas[i].contenido);
            }
            free(cache->entradas);
        }
        free(cache->algoritmo_reemplazo);
        free(cache->puntero_clock);
        free(cache);
    }
}
