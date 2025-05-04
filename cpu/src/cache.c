#include "../headers/cache.h"
#include "../headers/mmu.h"
#include "../headers/init.h"

t_cache_paginas* inicializar_cache(){
    cache = (t_cache_paginas*)malloc(sizeof(t_cache_paginas));
    if (cache == NULL){
        log_error(cpu_log,"No se pudo inicializar la cache");
        exit(EXIT_FAILURE);
    }
    cache->entradas = NULL;
    cache->cantidad_entradas = atoi(ENTRADAS_CACHE);
    if (cache_habilitada(cache)) {
        cache->entradas = (t_entrada_cache*)malloc(cache->cantidad_entradas * sizeof(t_entrada_cache));
        if(cache->entradas == NULL){
            log_error(cpu_log, "Error al asignar memoria para las entradas de la cache");
            free(cache);
            exit(EXIT_FAILURE);
        }
        for (int i = 0 ; i < cache->cantidad_entradas ; i++){
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
bool cache_habilitada(t_cache_paginas* cache){
    return cache->cantidad_entradas > 0;
}
int buscar_pagina_en_cache (t_cache_paginas* cache, int numero_pagina){
    if (!cache_habilitada(cache))
        return -1;
    for(int i = 0; i < cache->cantidad_entradas; i++){
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
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
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
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    } while(cache->puntero_clock != comienzo);
    return cache->puntero_clock;
}

char* acceder_a_pagina_en_cache(t_cache_paginas* cache, int numero_pagina){
    if(cache == NULL)
        return NULL;
    if (!cache_habilitada(cache)){
        log_info(cpu_log, "La cache esta deshabilitada.");
        return NULL;
    }
    int nro_pagina_en_cache = buscar_pagina_en_cache(cache,numero_pagina);
    if (nro_pagina_en_cache <= -1)
        return NULL;
    return cache->entradas[nro_pagina_en_cache].contenido;
}

void desalojar_proceso_cache(t_cache_paginas* cache){ 
    if (!cache_habilitada(cache)){
        log_error(cpu_log, "Cache deshabilitada");
        EXIT_FAILURE;
    }
    for (int i = 0; i < cache->cantidad_entradas; i++){
        log_info(cpu_log, "Limpiando cache");
        if (cache->entradas[i].modificado && !(cache->entradas[i].numero_pagina <= -1)){
            log_info(cpu_log, "Actualizando pagina modificada %d en memoria", cache->entradas[i].numero_pagina);
            //escribir_pagina_en_memoria();

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
    if(!cache_habilitada(cache)){
        log_info(cpu_log, "No hay entradas en la cache.");
        EXIT_SUCCESS;
    }
    for (int i = 0; i<cache->cantidad_entradas; i++){
        if (cache->entradas != NULL){
            for (int i = 0; i < cache->cantidad_entradas; i++)
            {
                free(cache->entradas[i].contenido);
            }
            free(cache->entradas);
        }
        free(cache->algoritmo_reemplazo);
        free(cache->puntero_clock); // warning: passing argument 1 of ‘free’ makes pointer from integer without a cast
        free(cache);
    }
}

void cache_modificar(uint32_t frame, char* datos){
    if (cache == NULL){
        log_info(cpu_log,"la cache ya estaba liberada.");
        return;
    }
    if (!cache_habilitada(cache)){
        log_info(cpu_log, "La cache esta deshabilitada.");
        return;
    }
    int nro_pagina_en_cache = buscar_pagina_en_cache(cache, frame);
    if (nro_pagina_en_cache <= -1){
        log_info(cpu_log, "No se encontro la pagina %d en la cache", frame);
        return;
    }
    cache->entradas[nro_pagina_en_cache].contenido = datos;
    cache->entradas[nro_pagina_en_cache].modificado = true;
}

void cache_escribir(uint32_t frame, char* datos){
    if (cache == NULL){
        log_info(cpu_log,"la cache ya estaba liberada.");
        
    }
    if (!cache_habilitada(cache)){
        log_info(cpu_log, "La cache esta deshabilitada.");
        
    }
    
    t_entrada_cache* entrada = malloc(sizeof(t_entrada_cache));
    entrada->numero_pagina = frame;
    entrada->contenido = datos;
    entrada->modificado = false;
    entrada->bit_referencia = 0;
    list_add(cache->entradas, entrada);
    
}