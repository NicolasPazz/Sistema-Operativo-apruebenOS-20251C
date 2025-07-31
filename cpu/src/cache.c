
#include "../headers/cache.h"
#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../headers/cicloDeInstruccion.h"
#include "../headers/main.h"
#include <unistd.h>

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
            // free(cache);
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
        log_error(cpu_log, "Error al asignar memoria para las entradas de la cache");
        // free(cache);
        exit(EXIT_FAILURE);
    }
    cache->puntero_clock = 0;
    return cache;
}
bool cache_habilitada() {
    return cache->cantidad_entradas > 0;
}

// Función para mostrar entradas de caché de forma compacta
void mostrar_entradas_cache_compacto() {
    if (cache == NULL || !cache_habilitada(cache)) {
        log_info(cpu_log, "Cache: [DESHABILITADA]");
        return;
    }
    
    char estado_cache[1024] = {0}; // Buffer más grande
    char entrada_str[128]; // Buffer más grande para entrada individual
    int offset = 0;
    
    // Usar snprintf en lugar de strcat para controlar límites
    offset = snprintf(estado_cache, sizeof(estado_cache), "Cache: [");
    
    for (int i = 0; i < cache->cantidad_entradas && offset < sizeof(estado_cache) - 100; i++) {
        if (cache->entradas[i].numero_pagina != -1) {
            snprintf(entrada_str, sizeof(entrada_str), "%sPag%d(%s%s)%s", 
                    (i > 0) ? " | " : "",
                    cache->entradas[i].numero_pagina,
                    cache->entradas[i].bit_referencia ? "R" : "",
                    cache->entradas[i].modificado ? "M" : "",
                    (i == cache->puntero_clock) ? "*" : "");
        } else {
            snprintf(entrada_str, sizeof(entrada_str), "%s[VACIA]%s", 
                    (i > 0) ? " | " : "",
                    (i == cache->puntero_clock) ? "*" : "");
        }
        
        // Verificar que hay espacio suficiente antes de concatenar
        int remaining = sizeof(estado_cache) - offset - 50; // Dejar espacio para el cierre
        if (strlen(entrada_str) < remaining) {
            offset += snprintf(estado_cache + offset, remaining, "%s", entrada_str);
        } else {
            break; // Salir si no hay más espacio
        }
    }
    
    // Agregar cierre de forma segura
    if (offset < sizeof(estado_cache) - 30) {
        snprintf(estado_cache + offset, sizeof(estado_cache) - offset, "] (* = puntero CLOCK)");
    }
    
    log_trace(cpu_log, "%s", estado_cache);
}

// Función para mostrar el estado de la caché
void mostrar_estado_cache_debug(const char* momento) {
    if (cache == NULL || !cache_habilitada(cache)) {
        log_trace(cpu_log, "=== ESTADO CACHÉ (%s) === CACHE DESHABILITADA", momento);
        return;
    }
    
    log_trace(cpu_log, "=== ESTADO CACHÉ (%s) === Puntero CLOCK: %d ===", momento, cache->puntero_clock);
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina != -1) {
            log_trace(cpu_log, "  [%d] PID=%d, Página=%d, R=%d, M=%s %s", 
                    i,
                    cache->entradas[i].pid,
                    cache->entradas[i].numero_pagina, 
                    cache->entradas[i].bit_referencia, 
                    cache->entradas[i].modificado ? "SÍ" : "NO",
                    (i == cache->puntero_clock) ? "<-- PUNTERO" : "");
        } else {
            log_trace(cpu_log, "  [%d] [VACÍA] %s", 
                    i, 
                    (i == cache->puntero_clock) ? "<-- PUNTERO" : "");
        }
    }
    log_trace(cpu_log, "=== FIN ESTADO CACHÉ ===");
}

// Aplica el retardo configurado para la caché
void aplicar_retardo_cache(void) {
    int retardo = atoi(RETARDO_CACHE);
    log_trace(cpu_log, "Aplicando retardo de cache: %d ms", retardo);
    if (retardo < 0) {
        log_error(cpu_log, "Retardo de cache negativo configurado: %d ms", retardo);
        return;
    }
    usleep(retardo * 1000);
}

int buscar_pagina_en_cache(int pid, int numero_pagina) {
    int resultado = -1;
    if (!cache_habilitada(cache)) {
        return -1;
    }
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina == numero_pagina && cache->entradas[i].pid == pid) {
            //log_info(cpu_log, "PID: %d - Cache Hit - Pagina: %d", pid, numero_pagina);
            if (strcmp(cache->algoritmo_reemplazo, "CLOCK") == 0 || strcmp(cache->algoritmo_reemplazo, "CLOCK-M") == 0) {
                cache->entradas[i].bit_referencia = 1;
                //mostrar_estado_cache_debug("DESPUÉS DE CACHE HIT - BIT_R=1");
            }
            resultado = i;
            break;
        }
    }
    return resultado;
}
// funcion para seleccionar "victima" de reemplazo (clock)
int seleccionar_victima_clock() {
    log_trace(cpu_log, "=== Iniciando Algoritmo CLOCK ===");
    log_trace(cpu_log, "Estado inicial - Puntero CLOCK en posición: %d", cache->puntero_clock);
    
    // Mostrar estado actual de la caché
    log_trace(cpu_log, "Estado de la caché antes del reemplazo:");
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina != -1) {
            log_trace(cpu_log, "  Entrada %d: PID=%d, Página=%d, Bit_R=%d, Modificado=%s %s", 
                    i, 
                    cache->entradas[i].pid,
                    cache->entradas[i].numero_pagina, 
                    cache->entradas[i].bit_referencia, 
                    cache->entradas[i].modificado ? "Sí" : "No",
                    (i == cache->puntero_clock) ? "<-- PUNTERO CLOCK" : "");
        } else {
            log_trace(cpu_log, "  Entrada %d: [VACÍA] %s", 
                    i, 
                    (i == cache->puntero_clock) ? "<-- PUNTERO CLOCK" : "");
        }
    }
    
    int iteraciones = 0;
    int victima = -1;
    
    // ALGORITMO CLOCK CORREGIDO: Resetear bits R=1 hasta encontrar R=0
    while (victima == -1) {
        iteraciones++;
        int pos_actual = cache->puntero_clock;
        t_entrada_cache* entrada_actual = &cache->entradas[pos_actual];
        
        log_trace(cpu_log, "CLOCK Iteración %d: Evaluando entrada %d (PID=%d, Página=%d, Bit_R=%d)", 
                iteraciones, 
                pos_actual,
                entrada_actual->pid,
                entrada_actual->numero_pagina, 
                entrada_actual->bit_referencia);
        
        if (entrada_actual->bit_referencia == 0) {
            // Encontramos la víctima
            log_info(cpu_log, PURPURA("VICTIMA SELECCIONADA: Entrada %d (PID=%d, Página=%d) - Motivo: Bit de uso = 0"), 
                    pos_actual,
                    entrada_actual->pid,
                    entrada_actual->numero_pagina);
            
            victima = pos_actual;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            log_trace(cpu_log, "Puntero CLOCK avanzado a posición: %d", cache->puntero_clock);
        } else {
            // Bit de uso = 1, lo ponemos en 0 y continuamos
            log_trace(cpu_log, "Entrada %d: Reseteando bit R (era R=1, ahora R=0) y continuando", 
                    pos_actual);
            entrada_actual->bit_referencia = 0;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            
            log_trace(cpu_log, "Bit de uso de entrada %d puesto en 0. Puntero CLOCK avanzado a posición: %d", 
                    pos_actual, cache->puntero_clock);
        }
        
        // Protección contra bucle infinito (aunque no debería pasar)
        if (iteraciones > cache->cantidad_entradas * 2) {
            log_trace(cpu_log, "ERROR: Algoritmo CLOCK en bucle infinito después de %d iteraciones", iteraciones);
            victima = cache->puntero_clock; // Tomar cualquiera como última instancia
            break;
        }
    }
    
    log_trace(cpu_log, "=== Fin Algoritmo CLOCK (Total iteraciones: %d) ===", iteraciones);
    return victima;
}


int seleccionar_victima_clock_m() {
    log_trace(cpu_log, "=== Iniciando Algoritmo CLOCK-M (CLOCK Mejorado) ===");
    log_trace(cpu_log, "Estado inicial - Puntero CLOCK en posición: %d", cache->puntero_clock);
    
    // Mostrar estado actual de la caché
    log_trace(cpu_log, "Estado de la caché antes del reemplazo:");
    for (int i = 0; i < cache->cantidad_entradas; i++) {
        if (cache->entradas[i].numero_pagina != -1) {
            log_trace(cpu_log, "  Entrada %d: PID=%d, Página=%d, Bit_R=%d, Modificado=%s %s", 
                    i, 
                    cache->entradas[i].pid,
                    cache->entradas[i].numero_pagina, 
                    cache->entradas[i].bit_referencia, 
                    cache->entradas[i].modificado ? "Sí" : "No",
                    (i == cache->puntero_clock) ? "<-- PUNTERO CLOCK" : "");
        } else {
            log_trace(cpu_log, "  Entrada %d: [VACÍA] %s", 
                    i, 
                    (i == cache->puntero_clock) ? "<-- PUNTERO CLOCK" : "");
        }
    }
    
    int iteraciones_total = 0;
    int ciclo = 1;
    
    // =================== BUCLE PRINCIPAL: Alternar entre Vuelta 1 y Vuelta 2 ===================
    while (true) {
        int comienzo = cache->puntero_clock;
        
        // =================== VUELTA 1: buscar (R=0, M=0) SIN MODIFICAR NADA ===================
        log_trace(cpu_log, "=== CICLO %d - VUELTA 1: Buscando páginas (R=0, M=0) - Sin modificar bits ===", ciclo);
        int iteraciones_vuelta1 = 0;
        
        do {
            iteraciones_total++;
            iteraciones_vuelta1++;
            int pos_actual = cache->puntero_clock;
            t_entrada_cache* entrada_actual = &cache->entradas[pos_actual];
            
            log_trace(cpu_log, "CLOCK-M Ciclo%d-V1 Iter.%d: Evaluando entrada %d (PID=%d, Página=%d, R=%d, M=%d)", 
                    ciclo, iteraciones_vuelta1,
                    pos_actual,
                    entrada_actual->pid,
                    entrada_actual->numero_pagina, 
                    entrada_actual->bit_referencia,
                    entrada_actual->modificado ? 1 : 0);
            
            if (entrada_actual->bit_referencia == 0 && !entrada_actual->modificado) {
                log_info(cpu_log, PURPURA("VICTIMA SELECCIONADA (Ciclo %d - Vuelta 1): Entrada %d (PID=%d, Página=%d) - Motivo: R=0 y M=0 (óptima)"), 
                        ciclo, pos_actual,
                        entrada_actual->pid,
                        entrada_actual->numero_pagina);
                
                int victima = cache->puntero_clock;
                cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
                log_trace(cpu_log, "Puntero CLOCK avanzado a posición: %d", cache->puntero_clock);
                log_trace(cpu_log, "=== Fin Algoritmo CLOCK-M (Total iteraciones: %d) ===", iteraciones_total);
                return victima;
            } else {
                log_trace(cpu_log, "Entrada %d NO es víctima - R=%d, M=%d (buscamos R=0 y M=0)", 
                        pos_actual, entrada_actual->bit_referencia, entrada_actual->modificado ? 1 : 0);
            }
            
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
        } while (cache->puntero_clock != comienzo);

        // =================== VUELTA 2: buscar solo (R=0, M=1) Y resetear R=1 sobre la marcha ===================
        log_trace(cpu_log, "=== CICLO %d - VUELTA 2: Buscando solo (R=0, M=1) y reseteando R=1 sobre la marcha ===", ciclo);
        int iteraciones_vuelta2 = 0;
        
        do {
            iteraciones_total++;
            iteraciones_vuelta2++;
            int pos_actual = cache->puntero_clock;
            t_entrada_cache* entrada_actual = &cache->entradas[pos_actual];
            
            log_trace(cpu_log, "CLOCK-M Ciclo%d-V2 Iter.%d: Evaluando entrada %d (PID=%d, Página=%d, R=%d, M=%d)", 
                    ciclo, iteraciones_vuelta2,
                    pos_actual,
                    entrada_actual->pid,
                    entrada_actual->numero_pagina, 
                    entrada_actual->bit_referencia,
                    entrada_actual->modificado ? 1 : 0);
            
            if (entrada_actual->bit_referencia == 0 && entrada_actual->modificado) {
                // Encontramos (R=0, M=1) - la víctima buscada
                log_info(cpu_log, PURPURA("VICTIMA SELECCIONADA (Ciclo %d - Vuelta 2): Entrada %d (PID=%d, Página=%d) - Motivo: R=0 y M=1"), 
                        ciclo, pos_actual,
                        entrada_actual->pid,
                        entrada_actual->numero_pagina);
                
                int victima = cache->puntero_clock;
                cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
                log_trace(cpu_log, "Puntero CLOCK avanzado a posición: %d", cache->puntero_clock);
                log_trace(cpu_log, "=== Fin Algoritmo CLOCK-M (Total iteraciones: %d) ===", iteraciones_total);
                return victima;
            } else if (entrada_actual->bit_referencia == 1) {
                // Resetear R=1 → R=0 y continuar (independientemente de M)
                log_trace(cpu_log, "Entrada %d: Reseteando R=1 → R=0 y continuando (Página %d, M=%d)", 
                        pos_actual, entrada_actual->numero_pagina, entrada_actual->modificado ? 1 : 0);
                entrada_actual->bit_referencia = 0;
            } else {
                // Es (R=0, M=0) - no es lo que buscamos en esta vuelta, continuar
                log_trace(cpu_log, "Entrada %d: R=0,M=0 encontrado pero esta vuelta busca R=0,M=1 - continuando", pos_actual);
            }

            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
        } while (cache->puntero_clock != comienzo);
        
        // Si llegamos aca, la vuelta 2 no encontró víctimas, volver a vuelta 1
        log_trace(cpu_log, "Ciclo %d completado sin éxito - iniciando ciclo %d", ciclo, ciclo + 1);
        ciclo++;
        
        // Protección contra bucle infinito
        if (ciclo > cache->cantidad_entradas * 5) {
            log_trace(cpu_log, "ERROR: CLOCK-M en bucle infinito después de %d ciclos", ciclo);
            // Tomar cualquiera como última instancia
            int victima = cache->puntero_clock;
            cache->puntero_clock = (cache->puntero_clock + 1) % cache->cantidad_entradas;
            log_trace(cpu_log, "=== Fin Algoritmo CLOCK-M (Total iteraciones: %d) ===", iteraciones_total);
            return victima;
        }
    }
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
    aplicar_retardo_cache();
    return resultado;
}

void desalojar_proceso_cache(int pid) {
    pthread_mutex_lock(&mutex_cache);
    if (!cache_habilitada(cache)) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "Cache deshabilitada");
        return;
    }

    mostrar_estado_cache_debug("ANTES DE DESALOJAR PROCESO");

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
    mostrar_estado_cache_debug("DESPUÉS DE DESALOJAR PROCESO");
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
        log_trace(cpu_log, "PID: %d - Contenido de caché nulo para página %d", pid, numero_pagina);
        pthread_mutex_unlock(&mutex_cache);
        return;
    }

    // Calcular offset dentro de la página
    int offset = direccion_logica % cfg_memoria->TAM_PAGINA;
    
    // Validar que la escritura no exceda los límites de la página
    if (offset + tamanio > cfg_memoria->TAM_PAGINA) {
        log_trace(cpu_log, "PID: %d - Escritura excede límites de página (offset=%d + tamaño=%d > TAM_PAGINA=%d)", 
                 pid, offset, tamanio, cfg_memoria->TAM_PAGINA);
        pthread_mutex_unlock(&mutex_cache);
        return;
    }
    
    // Modificar solo los bytes específicos (preservando el resto del contenido)
    memcpy(cache->entradas[pos].contenido + offset, datos, tamanio);
    cache->entradas[pos].modificado = true;

    log_trace(cpu_log, "PID: %d - Cache MODIFICADA: Página %d, Offset %d, Tamaño %d, Datos: '%s'", 
            pid, numero_pagina, offset, tamanio, datos);
    
    mostrar_estado_cache_debug("DESPUÉS DE MODIFICAR PÁGINA EN CACHÉ");
    
    // Log compacto de modificación
    log_info(cpu_log, AMARILLO("MODIFICACIÓN EN CACHE: Página %d"), numero_pagina);
    mostrar_entradas_cache_compacto();
    
    // Debug: mostrar contenido completo de la página después de la modificación
    char vista_pagina[65]; // Mostrar primeros 64 chars de la página
    int max_mostrar = cfg_memoria->TAM_PAGINA < 64 ? cfg_memoria->TAM_PAGINA : 64;
    memcpy(vista_pagina, cache->entradas[pos].contenido, max_mostrar);
    vista_pagina[max_mostrar] = '\0';
    log_trace(cpu_log, "PID: %d - Contenido página %d después de modificación: '%s'", pid, numero_pagina, vista_pagina);
    
    pthread_mutex_unlock(&mutex_cache);
    aplicar_retardo_cache();
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
        log_trace(cpu_log, "CACHE LLENA - Iniciando selección de víctima");
        mostrar_estado_cache_debug("ANTES DE REEMPLAZO - CACHÉ LLENA");
        
        entrada_index = (strcmp(cache->algoritmo_reemplazo, "CLOCK-M") == 0)
                        ? seleccionar_victima_clock_m()
                        : seleccionar_victima_clock();
        
        mostrar_estado_cache_debug("DESPUÉS DE SELECCIONAR VÍCTIMA");
        
        if (entrada_index == -1) {
            log_trace(cpu_log, "Error crítico: No se pudo seleccionar víctima en caché");
            pthread_mutex_unlock(&mutex_cache);
            return;
        }
        
        // Log informativo del reemplazo
        t_entrada_cache* entrada_victima = &cache->entradas[entrada_index];
        log_info(cpu_log, AZUL("REEMPLAZO: Página %d → Página %d"), entrada_victima->numero_pagina, frame);
        mostrar_entradas_cache_compacto();
    } else {
        // Log cuando hay entrada libre
        log_info(cpu_log, VERDE("CARGA DIRECTA: Página %d en entrada libre %d"), frame, entrada_index);
        mostrar_entradas_cache_compacto();
    }

    t_entrada_cache* entrada = &cache->entradas[entrada_index];

    // Procesar writeback si es necesario
    if (entrada->contenido != NULL &&
        entrada->modificado &&
        entrada->numero_pagina >= 0) {

        int pagina_vieja = entrada->numero_pagina;
        int pid_viejo = entrada->pid;
        
        log_trace(cpu_log, "Iniciando WRITEBACK: Página %d (PID=%d) será escrita a memoria antes del reemplazo", 
                pagina_vieja, pid_viejo);

        t_paquete* paquete = crear_paquete_op(ACCESO_TABLA_PAGINAS_OP);
        agregar_entero_a_paquete(paquete, pid_viejo);
        agregar_entero_a_paquete(paquete, pagina_vieja);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

        op_code codigo_operacion;
        if (recv(fd_memoria, &codigo_operacion, sizeof(op_code), MSG_WAITALL) != sizeof(op_code) ||
            codigo_operacion != PAQUETE_OP) {
            log_trace(cpu_log, "Error al obtener marco de memoria para (PID=%d, Página=%d)", pid_viejo, pagina_vieja);
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
                log_trace(cpu_log, "Cache Writeback: PID=%d Página=%d bajada a memoria", pid_viejo, pagina_vieja);
                mostrar_estado_cache_debug("DESPUÉS DE WRITEBACK A MEMORIA");
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
        log_trace(cpu_log, "Error al reservar memoria para caché (página %d)", frame);
        pthread_mutex_unlock(&mutex_cache);
        return;
    }

    // Inicializar toda la página con ceros
    memset(entrada->contenido, 0, tam_pagina);
    
    // Copiar datos de forma segura
    int datos_len = strlen(datos);
    if (datos_len > tam_pagina) {
        log_trace(cpu_log, "Datos exceden tamaño de página, truncando");
        datos_len = tam_pagina - 1;  // Dejar espacio para \0
    }
    memcpy(entrada->contenido, datos, datos_len);

    entrada->modificado = modificado;
    entrada->bit_referencia = 1;
    entrada->pid = pid;
    
    log_info(cpu_log, "PID: %d - Cache Add - Página: %d", pid, frame);
    log_trace(cpu_log, "CACHE CARGA COMPLETADA: Página %d (PID=%d) cargada en entrada %d - Estado: %s", 
            frame, pid, entrada_index, modificado ? "MODIFICADA" : "LIMPIA");

    mostrar_estado_cache_debug("DESPUÉS DE CARGAR NUEVA PÁGINA");
    
    // Log final compacto del estado
    log_info(cpu_log, VERDE("CACHE ACTUALIZADA:"));
    mostrar_entradas_cache_compacto();

    pthread_mutex_unlock(&mutex_cache);
    aplicar_retardo_cache();
}




char* cache_leer(int pid, int numero_pagina) {
    pthread_mutex_lock(&mutex_cache);
    int indice = buscar_pagina_en_cache(pid, numero_pagina);
    if (indice == -1) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "PID: %d - Cache Leer - Página %d no encontrada en caché", pid, numero_pagina);
        return NULL;
    }

    if (cache->entradas[indice].contenido == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "PID: %d - Cache Leer - Contenido nulo en entrada de página %d", pid, numero_pagina);
        return NULL;
    }

    char* copia = strdup(cache->entradas[indice].contenido);
    if (copia == NULL) {
        pthread_mutex_unlock(&mutex_cache);
        log_trace(cpu_log, "PID: %d - Error al duplicar contenido de caché para página %d", pid, numero_pagina);
        return NULL;
    }
    pthread_mutex_unlock(&mutex_cache);
    aplicar_retardo_cache();
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