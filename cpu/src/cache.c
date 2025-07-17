

#include "../headers/cache.h"
#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"
#include "../headers/main.h"

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

int buscar_pagina_en_cache(int pid, int numero_pagina) {
    int resultado = -1;
    if (!cache_habilitada(cache)) {
        return -1;
    }
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina == numero_pagina && cache->entradas[i].pid == pid) {
            log_info(cpu_log, "(PID: %d) - Cache Hit - Pagina: %d", pid, numero_pagina);
            if (strcmp(cache->algoritmo_reemplazo, "CLOCK") == 0 || strcmp(cache->algoritmo_reemplazo, "CLOCK-M") == 0) {
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
    log_info(cpu_log, "Se corre el Algoritmo de CLOCK");
    while (1) {
        if (cache->entradas[cache->puntero_clock].bit_referencia == 0) {
            int victima = cache->puntero_clock;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            return victima;
        }
        cache->entradas[cache->puntero_clock].bit_referencia = 0;
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    }
}


int seleccionar_victima_clock_m() {
    log_info(cpu_log, "Se corre el Algoritmo de CLOCK MEJORADO");
    int comienzo = cache->puntero_clock;

    // Primera pasada: buscar (R=0, M=0)
    do {
        if (cache->entradas[cache->puntero_clock].bit_referencia == 0 &&
            !cache->entradas[cache->puntero_clock].modificado) {
            int victima = cache->puntero_clock;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            return victima;
        }
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    } while (cache->puntero_clock != comienzo);

    // Segunda pasada: buscar (R=0, M=1) y limpiar R=1
    do {
        if (cache->entradas[cache->puntero_clock].bit_referencia == 0 &&
            cache->entradas[cache->puntero_clock].modificado) {
            int victima = cache->puntero_clock;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            return victima;
        }

        // Limpia bit de referencia en todos
        cache->entradas[cache->puntero_clock].bit_referencia = 0;

        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    } while (cache->puntero_clock != comienzo);

    // Tercera pasada: ya todos tienen R=0, elegir el primero disponible
    do {
        // Elegí cualquiera ahora, aunque tenga M=1
        int victima = cache->puntero_clock;
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
        return victima;
    } while (true);  // siempre encontrará uno, porque hay entradas
}


char* acceder_a_pagina_en_cache(int pid, int numero_pagina) {
    pthread_mutex_lock(&mutex_cache);
    if (cache == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        return NULL;
    }
    if (!cache_habilitada(cache)) {
        log_trace(cpu_log, "La cache esta deshabilitada.");
        pthread_mutex_unlock(&mutex_cache);
        return NULL;
    }
    int nro_pagina_en_cache = buscar_pagina_en_cache(pid, numero_pagina);
    char* resultado = NULL;
    if (nro_pagina_en_cache > -1)
        resultado = cache->entradas[nro_pagina_en_cache].contenido;
    pthread_mutex_unlock(&mutex_cache);
    return resultado;
}

void desalojar_proceso_cache(int pid) {
    pthread_mutex_lock(&mutex_cache);
    if (!cache_habilitada(cache)) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "Cache deshabilitada");
        return;
    }

    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].pid == pid) {

            log_trace(cpu_log, "Limpiando entrada de caché %d para PID %d", i, pid);

            if (cache->entradas[i].modificado && cache->entradas[i].numero_pagina >= 0) {
                int direccion_fisica = traducir_direccion_fisica(cache->entradas[i].numero_pagina * cfg_memoria->TAM_PAGINA);
                log_info(cpu_log, VERDE("(PID: %d) - Memory Update - Página: %d - Dir. Física: %d"),
                         pid, cache->entradas[i].numero_pagina, direccion_fisica);
                enviar_actualizar_pagina_completa(pid, direccion_fisica, cache->entradas[i].contenido);
            }

            free(cache->entradas[i].contenido);
            cache->entradas[i].contenido = NULL;
            cache->entradas[i].modificado = false;
            cache->entradas[i].bit_referencia = 0;
            cache->entradas[i].numero_pagina = -1;
            cache->entradas[i].pid = -1;
        }
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

void cache_modificar(int pid, int frame, char* datos) {
    pthread_mutex_lock(&mutex_cache);

    if (cache == NULL || !cache_habilitada(cache)) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "La cache está deshabilitada o ya fue liberada.");
        return;
    }

    int pos = buscar_pagina_en_cache(pid, frame);
    if (pos < 0) {
        log_trace(cpu_log, "No se encontró la página %d en la caché para PID %d", frame, pid);
        pthread_mutex_unlock(&mutex_cache);
        return;
    }

    if (cache->entradas[pos].contenido != NULL)
        free(cache->entradas[pos].contenido);

    int len = strlen(datos);
    cache->entradas[pos].contenido = malloc(len);
    memcpy(cache->entradas[pos].contenido, datos, len);
    cache->entradas[pos].modificado = true;
    aplicar_retardo_cache();
    log_trace(cpu_log, "Cache modificada (PID: %d, Pagina: %d, Tam: %d)", pid, frame, len);
    
    pthread_mutex_unlock(&mutex_cache);
}


void cache_escribir(int pid, int frame, char* datos, bool modificado) {
    pthread_mutex_lock(&mutex_cache);
    if (cache == NULL || !cache_habilitada(cache)) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "La cache está deshabilitada o ya fue liberada.");
        return;
    }

    int entrada_index = -1;

    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina == -1) {
            entrada_index = i;
            break;
        }
    }

    if (entrada_index == -1) {
        entrada_index = (strcmp(cache->algoritmo_reemplazo, "CLOCK-M") == 0)
                        ? seleccionar_victima_clock_m()
                        : seleccionar_victima_clock();
    }

    t_entrada_cache* entrada = &cache->entradas[entrada_index];

    if (entrada->contenido != NULL &&
        entrada->modificado &&
        entrada->numero_pagina >= 0) {

        int pagina_vieja = entrada->numero_pagina;
        int pid_viejo = entrada->pid;

        t_paquete* paquete = crear_paquete_op(ACCESO_TABLA_PAGINAS_OP);
        agregar_entero_a_paquete(paquete, pid_viejo);
        agregar_entero_a_paquete(paquete, pagina_vieja);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

        op_code codigo_operacion;
        if (recv(fd_memoria, &codigo_operacion, sizeof(op_code), MSG_WAITALL) != sizeof(op_code) ||
            codigo_operacion != PAQUETE_OP) {
            log_error(cpu_log, "Error al obtener marco de memoria para (PID=%d, Página=%d)", pid_viejo, pagina_vieja);
        } else {
            t_list* respuesta = recibir_contenido_paquete(fd_memoria);
            int marco = *(int*)list_get(respuesta, 0);
            list_destroy_and_destroy_elements(respuesta, free);

            if (marco != -1) {
                int direccion_fisica = marco * cfg_memoria->TAM_PAGINA;
                log_trace(cpu_log, "Escribiendo página vieja (PID=%d, Página=%d) en marco %d",
                          pid_viejo, pagina_vieja, marco);
                enviar_actualizar_pagina_completa(pid_viejo, direccion_fisica, entrada->contenido);
                log_info(cpu_log, "Cache Writeback: PID=%d Página=%d bajada a memoria", pid_viejo, pagina_vieja);
            } else {
                log_warning(cpu_log, "La página a desalojar (PID=%d, Página=%d) ya no está mapeada. No se actualiza.",
                            pid_viejo, pagina_vieja);
            }
        }
    }

    free(entrada->contenido);

    entrada->numero_pagina = frame;

    int size = strlen(datos) + 1;
    entrada->contenido = malloc(size);
    memcpy(entrada->contenido, datos, size);

    entrada->modificado = modificado;
    entrada->bit_referencia = 1;
    entrada->pid = pid;
    aplicar_retardo_cache();
    log_info(cpu_log, "(PID: %d) - Cache Add - Página: %d", pid, frame);

    pthread_mutex_unlock(&mutex_cache);
}




char* cache_leer(int pid, int numero_pagina) {
    pthread_mutex_lock(&mutex_cache);
    int indice = buscar_pagina_en_cache(pid, numero_pagina);
    if (indice == -1) {
        pthread_mutex_unlock(&mutex_cache);
        log_warning(cpu_log, "PID: %d - Cache Leer - Página %d no encontrada en caché", pid, numero_pagina);
        return NULL;
    }

    if (cache->entradas[indice].contenido == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_warning(cpu_log, "PID: %d - Cache Leer - Contenido nulo en entrada de página %d", pid, numero_pagina);
        return NULL;
    }

    char* copia = strdup(cache->entradas[indice].contenido);
    aplicar_retardo_cache();
    if (copia == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_error(cpu_log, "PID: %d - Error al duplicar contenido de caché para página %d", pid, numero_pagina);
        return NULL;
    }
    pthread_mutex_unlock(&mutex_cache);
    return copia;
}

void enviar_actualizar_pagina_completa(int pid, int direccion_fisica, void* contenido) {
    t_paquete* paquete = crear_paquete_op(ACTUALIZAR_PAGINA_COMPLETA_OP);

    char* pid_str = string_itoa(pid);
    char* dir_str = string_itoa(direccion_fisica);

    agregar_a_paquete(paquete, pid_str, strlen(pid_str) + 1);
    agregar_a_paquete(paquete, dir_str, strlen(dir_str) + 1);
    agregar_a_paquete(paquete, contenido, cfg_memoria->TAM_PAGINA);

    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    t_respuesta respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), MSG_WAITALL) != sizeof(t_respuesta)) {
        log_error(cpu_log, "Error al recibir respuesta de actualización de página completa");
        exit(EXIT_FAILURE);
    }

    if (respuesta != OK) {
        log_error(cpu_log, "Actualización de página completa fallida en Memoria");
        exit(EXIT_FAILURE);
    }

    free(pid_str);
    free(dir_str);
}
