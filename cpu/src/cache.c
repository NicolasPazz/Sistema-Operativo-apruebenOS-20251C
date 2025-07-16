#include "../headers/cache.h"
#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"
#include "../headers/main.h"
#include <unistd.h>
#include <stdlib.h>

static void aplicar_retardo_cache(void) {
    if (RETARDO_CACHE) {
        int retardo = atoi(RETARDO_CACHE);
        if (retardo > 0)
            usleep(retardo * 1000);
    }
}

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
int buscar_pagina_en_cache(int numero_pagina) {
    int resultado = -1;
    if (!cache_habilitada(cache)) {
        return -1;
    }
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina == numero_pagina) {
            log_info(cpu_log, VERDE("PID: %d - Cache Hit - Página: %d"), pid_ejecutando, numero_pagina);
            if (strcmp(cache->algoritmo_reemplazo,"CLOCK") == 0 || strcmp(cache->algoritmo_reemplazo, "CLOCK-M") == 0) {
                cache->entradas[i].bit_referencia = 1;
            }
            resultado = i;
            break;
        }
    }
    return resultado;
}
// funcion para seleccionar "victima" de reemplazo (clock)
int seleccionar_victima_clock() {
    while (1) {
        if (cache->entradas[cache->puntero_clock].bit_referencia == 0) {
            int victima = cache->puntero_clock;
            return victima;
        }
        cache->entradas[cache->puntero_clock].bit_referencia = 0;
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    }
}

int seleccionar_victima_clock_m () {
    int comienzo = cache->puntero_clock;
    // Primera pasada
    do {
        if (cache->entradas[cache->puntero_clock].bit_referencia == 0 && !cache->entradas[cache->puntero_clock].modificado) {
            return cache->puntero_clock;
        } 
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    } while (cache->puntero_clock != comienzo);

    // Segunda pasada
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
    pthread_mutex_lock(&mutex_cache);
    if (cache == NULL){
        pthread_mutex_unlock(&mutex_cache);
        return NULL;
    }
    if (!cache_habilitada(cache)) {
        log_trace(cpu_log, "La cache esta deshabilitada.");
        pthread_mutex_unlock(&mutex_cache);
        return NULL;
    }
    int nro_pagina_en_cache = buscar_pagina_en_cache(numero_pagina);
    char* resultado = NULL;
    if (nro_pagina_en_cache > -1)
        resultado = cache->entradas[nro_pagina_en_cache].contenido;
    pthread_mutex_unlock(&mutex_cache);
    return resultado;
}

void desalojar_proceso_cache() {
    pthread_mutex_lock(&mutex_cache);
    if (!cache_habilitada(cache)) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "Cache deshabilitada");
        return;
    }

    for (int i = 0; i < cache->cantidad_entradas; i++) {
        log_trace(cpu_log, "Limpiando cache");

        if (cache->entradas[i].modificado && cache->entradas[i].numero_pagina >= 0) {
            int frameC = -1;
            int pagina = cache->entradas[i].numero_pagina;
            
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
    pthread_mutex_unlock(&mutex_cache);
}

void liberar_cache() {
    pthread_mutex_lock(&mutex_cache);
    if (cache == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log,"la cache ya estaba liberada.");
        return;
    }
    if (!cache_habilitada(cache)) {
        log_trace(cpu_log, "No hay entradas en la cache.");
        free(cache);
        cache = NULL;
        pthread_mutex_unlock(&mutex_cache);
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
    pthread_mutex_unlock(&mutex_cache);
}

void cache_modificar(int frame, char* datos) {
    pthread_mutex_lock(&mutex_cache);
    if (cache == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log,"la cache ya estaba liberada.");
        return;
    }
    if (!cache_habilitada(cache)) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "La cache esta deshabilitada.");
        return;
    }
    int nro_pagina_en_cache = buscar_pagina_en_cache(frame); // Ojo, si ya tiene mutex, replanteá el diseño.
    if (nro_pagina_en_cache <= -1) {
        log_trace(cpu_log, "No se encontro la pagina %d en la cache", frame);
        pthread_mutex_unlock(&mutex_cache);
        return;
    }
    cache->entradas[nro_pagina_en_cache].contenido = strdup(datos);
    cache->entradas[nro_pagina_en_cache].modificado = true;
    aplicar_retardo_cache();
    log_trace(cpu_log, "PID: %d - Contenido leído (cache): %s", pid_ejecutando, cache->entradas[nro_pagina_en_cache].contenido);
    pthread_mutex_unlock(&mutex_cache);
}

void cache_escribir(int frame, char* datos) {
    pthread_mutex_lock(&mutex_cache);
    if (cache == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log,"la cache ya estaba liberada.");
        return;
    }
    if (!cache_habilitada(cache)) {
        pthread_mutex_unlock(&mutex_cache);
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
    cache->entradas[entrada_index].contenido = strdup(datos);
    cache->entradas[entrada_index].modificado = false;
    cache->entradas[entrada_index].bit_referencia = 1;
    aplicar_retardo_cache();
    log_info(cpu_log, VERDE("PID: %d - Cache Add - Página: %d"), pid_ejecutando, frame);
    pthread_mutex_unlock(&mutex_cache);
}

char* cache_leer(int numero_pagina) {
    pthread_mutex_lock(&mutex_cache);
    int indice = buscar_pagina_en_cache(numero_pagina);
    if (indice == -1) {
        pthread_mutex_unlock(&mutex_cache);
        log_debug(cpu_log, "PID: %d - Cache Leer - Página %d no encontrada en caché", pid_ejecutando, numero_pagina);
        return NULL;
    }

    if (cache->entradas[indice].contenido == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_debug(cpu_log, "PID: %d - Cache Leer - Contenido nulo en entrada de página %d", pid_ejecutando, numero_pagina);
        return NULL;
    }

    // Devolvemos una copia del contenido para que el caller pueda hacer free
    char* copia = strdup(cache->entradas[indice].contenido);
    aplicar_retardo_cache();
    if (copia == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_error(cpu_log, "PID: %d - Error al duplicar contenido de caché para página %d", pid_ejecutando, numero_pagina);
        return NULL;
    }
    pthread_mutex_unlock(&mutex_cache);
    return copia;
}