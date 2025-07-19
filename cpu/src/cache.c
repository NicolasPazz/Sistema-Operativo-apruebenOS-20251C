
#include "../headers/cache.h"
#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"
#include "../headers/main.h"

t_cache_paginas* inicializar_cache() {
    cache = (t_cache_paginas*)malloc(sizeof(t_cache_paginas));
    if (cache == NULL) {
        log_debug(cpu_log,"No se pudo inicializar la cache");
        exit(EXIT_FAILURE);
    }
    cache->entradas = NULL;
    cache->cantidad_entradas = atoi(ENTRADAS_CACHE);
    if (cache_habilitada(cache)) {
        cache->entradas = (t_entrada_cache*)malloc(cache->cantidad_entradas * sizeof(t_entrada_cache));
        if (cache->entradas == NULL) {
            log_debug(cpu_log, "Error al asignar memoria para las entradas de la cache");
            free(cache);
            exit(EXIT_FAILURE);
        }
        for (int i = 0 ; i < cache->cantidad_entradas ; i++) {
            cache->entradas[i].numero_pagina = -1;
            cache->entradas[i].contenido = NULL;
            cache->entradas[i].modificado = false;
            cache->entradas[i].bit_referencia = 0;
            cache->entradas[i].pid = -1;
        }
    }
    cache->algoritmo_reemplazo = REEMPLAZO_CACHE;
    if (cache->algoritmo_reemplazo == NULL) {
        log_debug(cpu_log, "Error al asignar memoria para las entradas de la cache");
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
            log_info(cpu_log, "PID: %d - Cache Hit - Pagina: %d", pid, numero_pagina);
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
    log_debug(cpu_log, "=== Iniciando Algoritmo CLOCK ===");
    log_debug(cpu_log, "Estado inicial - Puntero CLOCK en posición: %d", cache->puntero_clock);
    
    // Mostrar estado actual de la caché
    log_debug(cpu_log, "Estado de la caché antes del reemplazo:");
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina != -1) {
            log_debug(cpu_log, "  Entrada %d: PID=%d, Página=%d, Bit_R=%d, Modificado=%s %s", 
                    i, 
                    cache->entradas[i].pid,
                    cache->entradas[i].numero_pagina, 
                    cache->entradas[i].bit_referencia, 
                    cache->entradas[i].modificado ? "Sí" : "No",
                    (i == cache->puntero_clock) ? "<-- PUNTERO CLOCK" : "");
        } else {
            log_debug(cpu_log, "  Entrada %d: [VACÍA] %s", 
                    i, 
                    (i == cache->puntero_clock) ? "<-- PUNTERO CLOCK" : "");
        }
    }
    
    int iteraciones = 0;
    
    while (1) {
        iteraciones++;
        int pos_actual = cache->puntero_clock;
        t_entrada_cache* entrada_actual = &cache->entradas[pos_actual];
        
        log_debug(cpu_log, "CLOCK Iteración %d: Evaluando entrada %d (PID=%d, Página=%d, Bit_R=%d)", 
                iteraciones, 
                pos_actual,
                entrada_actual->pid,
                entrada_actual->numero_pagina, 
                entrada_actual->bit_referencia);
        
        if (entrada_actual->bit_referencia == 0) {
            // Encontramos la víctima
            log_info(cpu_log, PURPURA("VICTIMA SELECCIONADA: Entrada %d (PID=%d, Página=%d) - Motivo: Bit de referencia = 0"), 
                    pos_actual,
                    entrada_actual->pid,
                    entrada_actual->numero_pagina);
            
            int victima = cache->puntero_clock;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            
            log_debug(cpu_log, "Puntero CLOCK avanzado a posición: %d", cache->puntero_clock);
            log_debug(cpu_log, "=== Fin Algoritmo CLOCK (Total iteraciones: %d) ===", iteraciones);
            return victima;
        } else {
            // Bit de referencia = 1, lo ponemos en 0 y continuamos
            log_debug(cpu_log, "Entrada %d rechazada - Motivo: Bit de referencia = 1, cambiando a 0 y continuando", 
                    pos_actual);
            entrada_actual->bit_referencia = 0;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            
            log_debug(cpu_log, "Bit de referencia de entrada %d puesto en 0. Puntero CLOCK avanzado a posición: %d", 
                    pos_actual, cache->puntero_clock);
        }
        
        // Protección contra bucle infinito (aunque no debería pasar)
        if (iteraciones > cache->cantidad_entradas * 2) {
            log_debug(cpu_log, "ERROR: Algoritmo CLOCK en bucle infinito después de %d iteraciones", iteraciones);
            break;
        }
    }
    
    // Esta línea nunca debería ejecutarse
    log_debug(cpu_log, "ERROR CRÍTICO: Algoritmo CLOCK no pudo seleccionar víctima");
    return 0;
}


int seleccionar_victima_clock_m() {
    log_debug(cpu_log, "=== Iniciando Algoritmo CLOCK-M (CLOCK Mejorado) ===");
    log_debug(cpu_log, "Estado inicial - Puntero CLOCK en posición: %d", cache->puntero_clock);
    
    // Mostrar estado actual de la caché
    log_debug(cpu_log, "Estado de la caché antes del reemplazo:");
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina != -1) {
            log_debug(cpu_log, "  Entrada %d: PID=%d, Página=%d, Bit_R=%d, Modificado=%s %s", 
                    i, 
                    cache->entradas[i].pid,
                    cache->entradas[i].numero_pagina, 
                    cache->entradas[i].bit_referencia, 
                    cache->entradas[i].modificado ? "Sí" : "No",
                    (i == cache->puntero_clock) ? "<-- PUNTERO CLOCK" : "");
        } else {
            log_debug(cpu_log, "  Entrada %d: [VACÍA] %s", 
                    i, 
                    (i == cache->puntero_clock) ? "<-- PUNTERO CLOCK" : "");
        }
    }
    
    int comienzo = cache->puntero_clock;
    int iteraciones_total = 0;

    // =================== PRIMERA PASADA: buscar (R=0, M=0) ===================
    log_debug(cpu_log, "PASADA 1: Buscando páginas (R=0, M=0) - No referenciadas y no modificadas");
    int iteraciones_pasada1 = 0;
    
    do {
        iteraciones_total++;
        iteraciones_pasada1++;
        int pos_actual = cache->puntero_clock;
        t_entrada_cache* entrada_actual = &cache->entradas[pos_actual];
        
        log_debug(cpu_log, "CLOCK-M Pasada1 Iter.%d: Evaluando entrada %d (PID=%d, Página=%d, R=%d, M=%d)", 
                iteraciones_pasada1,
                pos_actual,
                entrada_actual->pid,
                entrada_actual->numero_pagina, 
                entrada_actual->bit_referencia,
                entrada_actual->modificado ? 1 : 0);
        
        if (entrada_actual->bit_referencia == 0 && !entrada_actual->modificado) {
            log_info(cpu_log, PURPURA("VICTIMA SELECCIONADA (Pasada 1): Entrada %d (PID=%d, Página=%d) - Motivo: R=0 y M=0 (óptima)"), 
                    pos_actual,
                    entrada_actual->pid,
                    entrada_actual->numero_pagina);
            
            int victima = cache->puntero_clock;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            log_debug(cpu_log, "Puntero CLOCK avanzado a posición: %d", cache->puntero_clock);
            log_debug(cpu_log, "=== Fin Algoritmo CLOCK-M (Total iteraciones: %d) ===", iteraciones_total);
            return victima;
        } else {
            log_debug(cpu_log, "Entrada %d rechazada - Motivo: R=%d o M=%d (buscamos R=0 y M=0)", 
                    pos_actual, entrada_actual->bit_referencia, entrada_actual->modificado ? 1 : 0);
        }
        
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    } while (cache->puntero_clock != comienzo);

    // =================== SEGUNDA PASADA: buscar (R=0, M=1) y limpiar R=1 ===================
    log_debug(cpu_log, "PASADA 2: Buscando páginas (R=0, M=1) y limpiando bits R=1");
    int iteraciones_pasada2 = 0;
    
    do {
        iteraciones_total++;
        iteraciones_pasada2++;
        int pos_actual = cache->puntero_clock;
        t_entrada_cache* entrada_actual = &cache->entradas[pos_actual];
        
        log_debug(cpu_log, "CLOCK-M Pasada2 Iter.%d: Evaluando entrada %d (PID=%d, Página=%d, R=%d, M=%d)", 
                iteraciones_pasada2,
                pos_actual,
                entrada_actual->pid,
                entrada_actual->numero_pagina, 
                entrada_actual->bit_referencia,
                entrada_actual->modificado ? 1 : 0);
        
        if (entrada_actual->bit_referencia == 0 && entrada_actual->modificado) {
            log_info(cpu_log, PURPURA("VICTIMA SELECCIONADA (Pasada 2): Entrada %d (PID=%d, Página=%d) - Motivo: R=0 y M=1"), 
                    pos_actual,
                    entrada_actual->pid,
                    entrada_actual->numero_pagina);
            
            int victima = cache->puntero_clock;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            log_debug(cpu_log, "Puntero CLOCK avanzado a posición: %d", cache->puntero_clock);
            log_debug(cpu_log, "=== Fin Algoritmo CLOCK-M (Total iteraciones: %d) ===", iteraciones_total);
            return victima;
        } else if (entrada_actual->bit_referencia == 1) {
            log_debug(cpu_log, "Entrada %d: Limpiando bit R (era R=1, ahora R=0) - Página %d", 
                    pos_actual, entrada_actual->numero_pagina);
            entrada_actual->bit_referencia = 0;
        } else {
            log_debug(cpu_log, "Entrada %d rechazada - Motivo: R=0 pero M=0 (buscamos R=0 y M=1)", pos_actual);
        }

        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
    } while (cache->puntero_clock != comienzo);

    // =================== TERCERA PASADA: elegir cualquiera (todos tienen R=0) ===================
    log_debug(cpu_log, "PASADA 3: Todos los bits R están en 0, eligiendo el primero disponible");
    int iteraciones_pasada3 = 0;
    
    do {
        iteraciones_total++;
        iteraciones_pasada3++;
        int pos_actual = cache->puntero_clock;
        t_entrada_cache* entrada_actual = &cache->entradas[pos_actual];
        
        log_debug(cpu_log, "CLOCK-M Pasada3 Iter.%d: Seleccionando entrada %d (PID=%d, Página=%d, R=%d, M=%d)", 
                iteraciones_pasada3,
                pos_actual,
                entrada_actual->pid,
                entrada_actual->numero_pagina, 
                entrada_actual->bit_referencia,
                entrada_actual->modificado ? 1 : 0);
        
        log_info(cpu_log, PURPURA("VICTIMA SELECCIONADA (Pasada 3): Entrada %d (PID=%d, Página=%d) - Motivo: Primera disponible (todos R=0)"), 
                pos_actual,
                entrada_actual->pid,
                entrada_actual->numero_pagina);
        
        int victima = cache->puntero_clock;
        cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
        log_debug(cpu_log, "Puntero CLOCK avanzado a posición: %d", cache->puntero_clock);
        log_debug(cpu_log, "=== Fin Algoritmo CLOCK-M (Total iteraciones: %d) ===", iteraciones_total);
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
                log_info(cpu_log, VERDE("PID: %d - Memory Update - Página: %d - Frame: %d"),
                         pid, cache->entradas[i].numero_pagina,(direccion_fisica - (cache->entradas[i].numero_pagina *cfg_memoria->TAM_PAGINA % cfg_memoria->TAM_PAGINA))/ cfg_memoria->TAM_PAGINA);
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

void cache_modificar(int pid, int numero_pagina, int direccion_logica, char* datos, int tamanio) {
    pthread_mutex_lock(&mutex_cache);

    if (cache == NULL || !cache_habilitada(cache)) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "La cache está deshabilitada o ya fue liberada.");
        return;
    }

    int pos = buscar_pagina_en_cache(pid, numero_pagina);
    if (pos < 0) {
        log_trace(cpu_log, "No se encontró la página %d en la caché para PID %d", numero_pagina, pid);
        pthread_mutex_unlock(&mutex_cache);
        return;
    }

    // Asegurar que tenemos contenido válido
    if (cache->entradas[pos].contenido == NULL) {
        log_debug(cpu_log, "PID: %d - Contenido de caché nulo para página %d", pid, numero_pagina);
        pthread_mutex_unlock(&mutex_cache);
        return;
    }

    // Calcular offset dentro de la página
    int offset = direccion_logica % cfg_memoria->TAM_PAGINA;
    
    // Validar que la escritura no exceda los límites de la página
    if (offset + tamanio > cfg_memoria->TAM_PAGINA) {
        log_debug(cpu_log, "PID: %d - Escritura excede límites de página (offset=%d + tamaño=%d > TAM_PAGINA=%d)", 
                 pid, offset, tamanio, cfg_memoria->TAM_PAGINA);
        pthread_mutex_unlock(&mutex_cache);
        return;
    }
    
    // Modificar solo los bytes específicos (preservando el resto del contenido)
    memcpy(cache->entradas[pos].contenido + offset, datos, tamanio);
    cache->entradas[pos].modificado = true;

    log_debug(cpu_log, "PID: %d - Cache MODIFICADA: Página %d, Offset %d, Tamaño %d, Datos: '%s'", 
            pid, numero_pagina, offset, tamanio, datos);
    
    // Debug: mostrar contenido completo de la página después de la modificación
    char vista_pagina[65]; // Mostrar primeros 64 chars de la página
    int max_mostrar = cfg_memoria->TAM_PAGINA < 64 ? cfg_memoria->TAM_PAGINA : 64;
    memcpy(vista_pagina, cache->entradas[pos].contenido, max_mostrar);
    vista_pagina[max_mostrar] = '\0';
    log_trace(cpu_log, "PID: %d - Contenido página %d después de modificación: '%s'", pid, numero_pagina, vista_pagina);
    
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

    // Buscar entrada libre
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina == -1) {
            entrada_index = i;
            break;
        }
    }

    // Si no hay entradas libres, seleccionar víctima
    if (entrada_index == -1) {
        log_debug(cpu_log, "CACHE LLENA - Iniciando selección de víctima");
        entrada_index = (strcmp(cache->algoritmo_reemplazo, "CLOCK-M") == 0)
                        ? seleccionar_victima_clock_m()
                        : seleccionar_victima_clock();
        
        if (entrada_index == -1) {
            log_debug(cpu_log, "Error crítico: No se pudo seleccionar víctima en caché");
            pthread_mutex_unlock(&mutex_cache);
            return;
        }
    }

    t_entrada_cache* entrada = &cache->entradas[entrada_index];

    // Procesar writeback si es necesario
    if (entrada->contenido != NULL &&
        entrada->modificado &&
        entrada->numero_pagina >= 0) {

        int pagina_vieja = entrada->numero_pagina;
        int pid_viejo = entrada->pid;
        
        log_debug(cpu_log, "Iniciando WRITEBACK: Página %d (PID=%d) será escrita a memoria antes del reemplazo", 
                pagina_vieja, pid_viejo);

        t_paquete* paquete = crear_paquete_op(ACCESO_TABLA_PAGINAS_OP);
        agregar_entero_a_paquete(paquete, pid_viejo);
        agregar_entero_a_paquete(paquete, pagina_vieja);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

        op_code codigo_operacion;
        if (recv(fd_memoria, &codigo_operacion, sizeof(op_code), MSG_WAITALL) != sizeof(op_code) ||
            codigo_operacion != PAQUETE_OP) {
            log_debug(cpu_log, "Error al obtener marco de memoria para (PID=%d, Página=%d)", pid_viejo, pagina_vieja);
        } else {
            t_list* respuesta = recibir_contenido_paquete(fd_memoria);
            int marco = *(int*)list_get(respuesta, 0);
            list_destroy_and_destroy_elements(respuesta, free);

            if (marco != -1) {
                int direccion_fisica = marco * cfg_memoria->TAM_PAGINA;
                log_trace(cpu_log, "Escribiendo página vieja (PID=%d, Página=%d) en marco %d",
                          pid_viejo, pagina_vieja, marco);
                enviar_actualizar_pagina_completa(pid_viejo, direccion_fisica, entrada->contenido);
                // Log obligatorio Memory Update
                log_info(cpu_log, VERDE("PID: %d - Memory Update - Página: %d - Frame: %d"), pid_viejo, pagina_vieja, marco);
                log_debug(cpu_log, "Cache Writeback: PID=%d Página=%d bajada a memoria", pid_viejo, pagina_vieja);
            } else {
                log_info(cpu_log, "La página a desalojar (PID=%d, Página=%d) ya no está mapeada. No se actualiza.",
                            pid_viejo, pagina_vieja);
            }
        }
    }

    // Liberar contenido anterior si existe
    if (entrada->contenido != NULL) {
        free(entrada->contenido);
    }

    entrada->numero_pagina = frame;

    // SIEMPRE reservar el tamaño completo de la página
    int tam_pagina = cfg_memoria->TAM_PAGINA;
    entrada->contenido = malloc(tam_pagina);
    if (!entrada->contenido) {
        log_debug(cpu_log, "Error al reservar memoria para caché (página %d)", frame);
        pthread_mutex_unlock(&mutex_cache);
        return;
    }

    // Inicializar toda la página con ceros
    memset(entrada->contenido, 0, tam_pagina);
    
    // Copiar datos de forma segura
    int datos_len = strlen(datos);
    if (datos_len > tam_pagina) {
        log_debug(cpu_log, "Datos exceden tamaño de página, truncando");
        datos_len = tam_pagina - 1;  // Dejar espacio para \0
    }
    memcpy(entrada->contenido, datos, datos_len);

    entrada->modificado = modificado;
    entrada->bit_referencia = 1;
    entrada->pid = pid;
    
    log_info(cpu_log, "PID: %d - Cache Add - Página: %d", pid, frame);
    log_debug(cpu_log, "CACHE CARGA COMPLETADA: Página %d (PID=%d) cargada en entrada %d - Estado: %s", 
            frame, pid, entrada_index, modificado ? "MODIFICADA" : "LIMPIA");

    pthread_mutex_unlock(&mutex_cache);
}




char* cache_leer(int pid, int numero_pagina) {
    pthread_mutex_lock(&mutex_cache);
    int indice = buscar_pagina_en_cache(pid, numero_pagina);
    if (indice == -1) {
        pthread_mutex_unlock(&mutex_cache);
        log_debug(cpu_log, "PID: %d - Cache Leer - Página %d no encontrada en caché", pid, numero_pagina);
        return NULL;
    }

    if (cache->entradas[indice].contenido == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_debug(cpu_log, "PID: %d - Cache Leer - Contenido nulo en entrada de página %d", pid, numero_pagina);
        return NULL;
    }

    char* copia = strdup(cache->entradas[indice].contenido);
    if (copia == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_debug(cpu_log, "PID: %d - Error al duplicar contenido de caché para página %d", pid, numero_pagina);
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
        log_debug(cpu_log, "Error al recibir respuesta de actualización de página completa");
        exit(EXIT_FAILURE);
    }

    if (respuesta != OK) {
        log_debug(cpu_log, "Actualización de página completa fallida en Memoria");
        exit(EXIT_FAILURE);
    }

    free(pid_str);
    free(dir_str);
}