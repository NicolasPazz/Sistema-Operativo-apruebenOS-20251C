#include "../headers/manejo_swap.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/metricas.h"
#include "../headers/manejo_memoria.h"
#include "../headers/bloqueo_paginas.h"
#include <commons/log.h>
#include <commons/string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

// Variables externas
extern t_sistema_memoria* sistema_memoria;
extern t_log* logger;
extern t_config_memoria* cfg;

// ============================================================================
// FUNCIONES DE SUSPENSIÓN Y REANUDACIÓN DE PROCESOS
// ============================================================================

int suspender_proceso_completo(int pid) {
    log_trace(logger, "[SWAP] >>>>> ENTRANDO a suspender_proceso_completo para PID %d", pid);
    if (!proceso_existe(pid)) {
        log_error(logger, "[SWAP] PID: %d - Proceso no existe, no se puede suspender", pid);
        return 0;
    }
    
    log_trace(logger, "[SWAP] PID: %d - Iniciando suspensión completa del proceso", pid);
    
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, string_itoa(pid));
    if (proceso == NULL || proceso->suspendido) {
        log_warning(logger, "[SWAP] PID: %d - Proceso ya suspendido o no encontrado", pid);
        return 0;
    }
    
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    log_trace(logger, "[SWAP] PID: %d - mutex_swap adquirido", pid);
    
    // Obtener estructura de páginas del proceso
    t_estructura_paginas* estructura = proceso->estructura_paginas;
    if (estructura == NULL) {
        log_error(logger, "[SWAP] PID: %d - No se encontró estructura de páginas", pid);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Verificar espacio en SWAP ANTES de comenzar la suspensión
    if (!asignar_espacio_swap_proceso(pid)) {
        log_error(logger, "[SWAP] PID: %d - No hay suficiente espacio en SWAP", pid);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    int paginas_escritas = 0;
    int paginas_omitidas = 0;
    bool suspension_exitosa = true;
    log_trace(logger, "[SWAP] PID: %d - Comenzando bucle de páginas (totales: %d)", pid, estructura->paginas_totales);
    int marcos_libres_antes = sistema_memoria->admin_marcos->frames_libres;
    int marcos_liberados = 0;
    int paginas_con_presente = 0;
    int paginas_con_frame = 0;
    for (int i = 0; i < estructura->paginas_totales && suspension_exitosa; i++) {
        t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, i);
        log_trace(logger, "[SWAP] PID: %d - Página %d: entrada %p, presente=%d", pid, i, entrada, entrada ? entrada->presente : -1);
        if (entrada != NULL && entrada->presente) {
            void* contenido_pagina = malloc(cfg->TAM_PAGINA);
            if (contenido_pagina == NULL) {
                log_error(logger, "[SWAP] Error al asignar memoria para página %d del proceso %d", i, pid);
                suspension_exitosa = false;
                break;
            }
            log_trace(logger, "[SWAP] PID: %d - Leyendo página %d del marco %d", pid, i, entrada->numero_frame);
            if (leer_pagina_memoria(entrada->numero_frame, contenido_pagina) != MEMORIA_OK) {
                log_error(logger, "[SWAP] Error al leer página %d del proceso %d", i, pid);
                free(contenido_pagina);
                suspension_exitosa = false;
                break;
            }
            log_trace(logger, "[SWAP] PID: %d - Escribiendo página %d a SWAP", pid, i);
            if (!escribir_pagina_proceso_swap(pid, i, contenido_pagina)) {
                log_error(logger, "[SWAP] Error al escribir página %d del proceso %d a SWAP", i, pid);
                free(contenido_pagina);
                suspension_exitosa = false;
                break;
            }
            int frame_a_liberar = entrada->numero_frame; // Guardar antes de modificar
            log_trace(logger, "[SWAP] PID: %d - Liberando marco %d", pid, frame_a_liberar);
            liberar_marco(frame_a_liberar);
            // Ahora marcar la entrada como no presente y limpiar el frame
            entrada->presente = false;
            entrada->numero_frame = 0;
            paginas_escritas++;
            free(contenido_pagina);
            log_trace(logger, "[SWAP] PID: %d - Página %d suspendida exitosamente", pid, i);
        } else {
            log_trace(logger, "[SWAP] PID: %d - Página %d no presente, se omite (motivo: %s)", pid, i, entrada == NULL ? "entrada nula" : "no presente");
            paginas_omitidas++;
        }
        // Verificación: después de liberar, la entrada debe tener presente=false y numero_frame=0
        if (entrada) {
            if (entrada->presente) paginas_con_presente++;
            if (entrada->numero_frame != 0) paginas_con_frame++;
        }
    }
    int marcos_libres_despues = sistema_memoria->admin_marcos->frames_libres;
    marcos_liberados = marcos_libres_despues - marcos_libres_antes;
    if (marcos_liberados != paginas_escritas) {
        log_error(logger, "[SWAP][CHECK] PID: %d - Marcos liberados (%d) != páginas escritas a swap (%d)", pid, marcos_liberados, paginas_escritas);
    } else {
        log_info(logger, "[SWAP][CHECK] PID: %d - Marcos liberados correctamente: %d", pid, marcos_liberados);
    }
    if (paginas_con_presente > 0 || paginas_con_frame > 0) {
        log_error(logger, "[SWAP][CHECK] PID: %d - %d páginas quedaron con presente=true y %d con numero_frame!=0 tras suspensión", pid, paginas_con_presente, paginas_con_frame);
    } else {
        log_info(logger, "[SWAP][CHECK] PID: %d - Todas las entradas de página correctamente con presente=false y numero_frame=0", pid);
    }
    if (suspension_exitosa) {
        proceso->suspendido = true;
        incrementar_bajadas_swap(pid);
        log_info(logger, "[SWAP] PID: %d - Proceso suspendido completamente. Páginas escritas a SWAP: %d, omitidas: %d", pid, paginas_escritas, paginas_omitidas);
        
    } else {
        log_error(logger, "[SWAP] PID: %d - Falló la suspensión, liberando páginas de SWAP", pid);
        liberar_espacio_swap_proceso(pid);
    }
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    log_trace(logger, "[SWAP] PID: %d - mutex_swap liberado", pid);
    log_trace(logger, "[SWAP] PID: %d - Aplicando retardo SWAP (solo una vez por suspensión)", pid);
    aplicar_retardo_swap();
    log_trace(logger, "[SWAP] <<<<< SALIENDO de suspender_proceso_completo para PID %d (resultado: %d)", pid, suspension_exitosa ? 1 : 0);
    return suspension_exitosa ? 1 : 0;
}

int reanudar_proceso_suspendido(int pid) {
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe, no se puede reanudar", pid);
        return 0;
    }
    
    log_trace(logger, "PID: %d - Iniciando reanudación del proceso suspendido", pid);
    
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, string_itoa(pid));
    if (proceso == NULL || !proceso->suspendido) {
        log_warning(logger, "PID: %d - Proceso no está suspendido", pid);
        return 0;
    }
    
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Obtener estructura de páginas del proceso
    t_estructura_paginas* estructura = proceso->estructura_paginas;
    if (estructura == NULL) {
        log_error(logger, "PID: %d - No se encontró estructura de páginas", pid);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Verificar si hay espacio suficiente en memoria principal
    if (sistema_memoria->admin_marcos->frames_libres < estructura->paginas_totales) {
        log_error(logger, "PID: %d - No hay suficiente espacio en memoria para reanudar proceso", pid);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        return 0;
    }
    
    // Leer TODAS las páginas desde SWAP usando mapeo específico
    int paginas_cargadas = 0;
    int paginas_fallidas = 0;
    bool reanudacion_exitosa = true;
    int pagina_actual = 0; // Declarar variable para el bucle de limpieza
    
    for (pagina_actual = 0; pagina_actual < estructura->paginas_totales && reanudacion_exitosa; pagina_actual++) {
        t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, pagina_actual);
        
        if (entrada != NULL && !entrada->presente) {
            // Asignar nuevo frame en memoria principal
            int nuevo_frame = asignar_marco_libre(pid, pagina_actual);
            if (nuevo_frame == -1) {
                log_error(logger, "PID: %d - No se pudo asignar frame para página %d", pid, pagina_actual);
                reanudacion_exitosa = false;
                break;
            }
            // Marcar la entrada como presente y setear el frame ANTES de leer swap
            entrada->presente = true;
            entrada->numero_frame = nuevo_frame;
            entrada->timestamp_acceso = time(NULL);

            void* contenido = leer_pagina_proceso_swap(pid, pagina_actual);
            if (contenido == NULL) {
                log_error(logger, "PID: %d - Error al leer página %d desde SWAP", pid, pagina_actual);
                // Revertir entrada y liberar marco
                entrada->presente = false;
                entrada->numero_frame = 0;
                liberar_marco(nuevo_frame);
                paginas_fallidas++;
                reanudacion_exitosa = false;
                break;
            }
            // Escribir contenido en memoria principal
            if (escribir_pagina_memoria(nuevo_frame, contenido) != MEMORIA_OK) {
                log_error(logger, "PID: %d - Error al escribir página %d en memoria", pid, pagina_actual);
                free(contenido);
                entrada->presente = false;
                entrada->numero_frame = 0;
                liberar_marco(nuevo_frame);
                paginas_fallidas++;
                reanudacion_exitosa = false;
                break;
            }
            paginas_cargadas++;
            free(contenido);
            log_trace(logger, "PID: %d - Página %d reanudada exitosamente en marco %d", pid, pagina_actual, nuevo_frame);
        } else if (entrada == NULL) {
            log_error(logger, "PID: %d - Entrada de tabla nula para página %d al reanudar", pid, pagina_actual);
            paginas_fallidas++;
        } else if (entrada->presente) {
            log_trace(logger, "PID: %d - Página %d ya presente en memoria al reanudar", pid, pagina_actual);
        }
    }
    
    if (reanudacion_exitosa) {
        // Liberar espacio en SWAP (ya no se necesita)
        liberar_espacio_swap_proceso(pid);
        
        // Marcar proceso como activo
        proceso->suspendido = false;
        
        // Incrementar métrica de subidas a memoria principal
        incrementar_subidas_memoria_principal(pid);
        
        log_info(logger, "PID: %d - Proceso reanudado exitosamente. Páginas cargadas desde SWAP: %d, fallidas: %d", 
                 pid, paginas_cargadas, paginas_fallidas);
    } else {
        // Si falló la reanudación, liberar los marcos ya asignados en esta operación
        log_error(logger, "PID: %d - Falló la reanudación, liberando marcos asignados", pid);
        for (int j = 0; j < pagina_actual; j++) {
            t_entrada_tabla* entrada_liberar = buscar_entrada_tabla(estructura, j);
            if (entrada_liberar && entrada_liberar->presente) {
                liberar_marco(entrada_liberar->numero_frame);
                entrada_liberar->presente = false;
                entrada_liberar->numero_frame = 0;
            }
        }
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Aplicar retardo de SWAP
    aplicar_retardo_swap();
    
    return reanudacion_exitosa ? 1 : 0;
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE PÁGINAS EN SWAP
// ============================================================================

int escribir_pagina_proceso_swap(int pid, int numero_pagina, void* contenido) {
    // BLOQUEAR PÁGINA PARA OPERACIÓN DE SWAP
    if (!bloquear_marco_por_pagina(pid, numero_pagina, "ESCRITURA_SWAP")) {
        log_error(logger, "PID: %d - No se pudo bloquear página %d para escritura en SWAP", pid, numero_pagina);
        return 0;
    }
    // NO BLOQUEAR EL MUTEX ACA, ya está bloqueado afuera
    // pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);

    // Loguear info del swapfile y fd
    log_trace(logger, "[DEBUG SWAP] PATH_SWAPFILE='%s', fd_swap=%d", cfg->PATH_SWAPFILE, sistema_memoria->admin_swap->fd_swap);

    // Buffer temporal para la página
    char buffer_pagina[4096];  // Buffer suficiente para cualquier tamaño de página

    // Obtener el marco físico donde está la página - Buscar en la estructura del proceso
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);

    if (!proceso || !proceso->estructura_paginas) {
        log_error(logger, "PID: %d - Proceso no encontrado para escribir página %d", pid, numero_pagina);
        desbloquear_marco_por_pagina(pid, numero_pagina, "ESCRITURA_SWAP");
        return 0;
    }

    // Buscar la entrada de tabla para obtener el marco físico
    t_entrada_tabla* entrada = buscar_entrada_tabla(proceso->estructura_paginas, numero_pagina);
    if (!entrada || !entrada->presente) {
        log_trace(logger, "PID: %d - Página %d no está presente en memoria, se omite escritura a SWAP", pid, numero_pagina);
        desbloquear_marco_por_pagina(pid, numero_pagina, "ESCRITURA_SWAP");
        return 1; // Considerar éxito, no error
    }

    int numero_marco = entrada->numero_frame;

    // Validar que el marco tiene el tamaño correcto
    if (cfg->TAM_PAGINA > sizeof(buffer_pagina)) {
        log_error(logger, "Tamaño de página (%d) excede el buffer (%d)", cfg->TAM_PAGINA, (int)sizeof(buffer_pagina));
        desbloquear_marco_por_pagina(pid, numero_pagina, "ESCRITURA_SWAP");
        return 0;
    }

    // Log antes de memcpy
    log_trace(logger, "[DEBUG SWAP] memcpy: origen=%p (memoria_principal + %d*%d), destino=%p, size=%d", 
        sistema_memoria->memoria_principal + (numero_marco * cfg->TAM_PAGINA), numero_marco, cfg->TAM_PAGINA, buffer_pagina, cfg->TAM_PAGINA);
    void* direccion_pagina = sistema_memoria->memoria_principal + (numero_marco * cfg->TAM_PAGINA);
    memcpy(buffer_pagina, direccion_pagina, cfg->TAM_PAGINA);
    log_trace(logger, "[DEBUG SWAP] memcpy OK para PID %d, pagina %d", pid, numero_pagina);

    // Log antes de write
    log_trace(logger, "[DEBUG SWAP] write: fd=%d, size=%d", sistema_memoria->admin_swap->fd_swap, cfg->TAM_PAGINA);
    ssize_t bytes_escritos = write(sistema_memoria->admin_swap->fd_swap, buffer_pagina, cfg->TAM_PAGINA);
    if (bytes_escritos != cfg->TAM_PAGINA) {
        log_error(logger, "Error al escribir página a SWAP: solo se escribieron %zd de %d bytes. errno=%d (%s)", 
                  bytes_escritos, cfg->TAM_PAGINA, errno, strerror(errno));
        desbloquear_marco_por_pagina(pid, numero_pagina, "ESCRITURA_SWAP");
        return 0;
    }
    log_trace(logger, "[DEBUG SWAP] write OK para PID %d, pagina %d, offset actual=%ld", pid, numero_pagina, (long)lseek(sistema_memoria->admin_swap->fd_swap, 0, SEEK_CUR));

    // Actualizar información de SWAP
    entrada->presente = false;  // Ya no está en memoria principal
    entrada->timestamp_acceso = time(NULL);

    // Incrementar métrica SOLO si la operación fue exitosa
    incrementar_bajadas_swap(pid);

    log_trace(logger, "PID: %d - Página %d escrita en SWAP en posición %ld", 
              pid, numero_pagina, (long)lseek(sistema_memoria->admin_swap->fd_swap, 0, SEEK_CUR));

    // DESBLOQUEAR PÁGINA AL FINALIZAR
    desbloquear_marco_por_pagina(pid, numero_pagina, "ESCRITURA_SWAP");

    // NO aplicar retardo aquí
    // aplicar_retardo_swap();

    return 1;
}

void* leer_pagina_proceso_swap(int pid, int numero_pagina) {
    // BLOQUEAR PÁGINA PARA OPERACIÓN DE SWAP
    if (!bloquear_marco_por_pagina(pid, numero_pagina, "LECTURA_SWAP")) {
        log_error(logger, "PID: %d - No se pudo bloquear página %d para lectura desde SWAP", pid, numero_pagina);
        return NULL;
    }
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    // Buscar la página en SWAP
    int posicion_swap = -1;
    for (int i = 0; i < sistema_memoria->admin_swap->cantidad_paginas_swap; i++) {
        if (sistema_memoria->admin_swap->entradas[i].ocupado && 
            sistema_memoria->admin_swap->entradas[i].pid_propietario == pid &&
            sistema_memoria->admin_swap->entradas[i].numero_pagina == numero_pagina) {
            posicion_swap = i;
            break;
        }
    }
    if (posicion_swap == -1) {
        log_error(logger, "PID: %d - Página %d no encontrada en SWAP", pid, numero_pagina);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        desbloquear_marco_por_pagina(pid, numero_pagina, "LECTURA_SWAP");
        return NULL;
    }
    // Asignar memoria para el contenido
    void* contenido = malloc(cfg->TAM_PAGINA);
    if (contenido == NULL) {
        log_error(logger, "PID: %d - Error al asignar memoria para leer página %d", pid, numero_pagina);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        desbloquear_marco_por_pagina(pid, numero_pagina, "LECTURA_SWAP");
        return NULL;
    }
    // Calcular offset y leer desde el archivo SWAP
    off_t offset = posicion_swap * cfg->TAM_PAGINA;
    if (lseek(sistema_memoria->admin_swap->fd_swap, offset, SEEK_SET) == -1) {
        log_error(logger, "PID: %d - Error al posicionarse en SWAP para leer página %d", pid, numero_pagina);
        free(contenido);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        desbloquear_marco_por_pagina(pid, numero_pagina, "LECTURA_SWAP");
        return NULL;
    }
    if (read(sistema_memoria->admin_swap->fd_swap, contenido, cfg->TAM_PAGINA) != cfg->TAM_PAGINA) {
        log_error(logger, "PID: %d - Error al leer página %d desde SWAP", pid, numero_pagina);
        free(contenido);
        pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
        desbloquear_marco_por_pagina(pid, numero_pagina, "LECTURA_SWAP");
        return NULL;
    }
    // Incrementar métrica de subidas desde SWAP
    incrementar_subidas_memoria_principal(pid);
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    // NO aplicar retardo aquí
    // aplicar_retardo_swap();
    // DESBLOQUEAR PÁGINA AL FINALIZAR
    desbloquear_marco_por_pagina(pid, numero_pagina, "LECTURA_SWAP");
    log_trace(logger, "PID: %d - Página %d leída desde SWAP posición %d", 
              pid, numero_pagina, posicion_swap);
    return contenido;
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE ESPACIO EN SWAP
// ============================================================================

// --- Cambiar implementación de asignar_espacio_swap_proceso para NO bloquear el mutex ---
int asignar_espacio_swap_proceso(int pid) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    
    if (proceso == NULL) {
        log_error(logger, "PID: %d - Proceso no encontrado para asignar SWAP", pid);
        return 0;
    }
    
    int paginas_necesarias = proceso->estructura_paginas->paginas_totales;
    // NO BLOQUEAR EL MUTEX ACA porq ya está bloqueado afuera
    if (sistema_memoria->admin_swap->paginas_libres_swap < paginas_necesarias) {
        log_error(logger, "PID: %d - No hay suficiente espacio en SWAP (%d páginas necesarias, %d disponibles)", 
                  pid, paginas_necesarias, sistema_memoria->admin_swap->paginas_libres_swap);
        return 0;
    }
    log_trace(logger, "PID: %d - Espacio en SWAP verificado: %d páginas disponibles", 
              pid, paginas_necesarias);
    return 1;
}

void liberar_espacio_swap_proceso(int pid) {
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    int paginas_liberadas = 0;
    
    // Buscar y liberar todas las páginas del proceso en SWAP
    for (int i = 0; i < sistema_memoria->admin_swap->cantidad_paginas_swap; i++) {
        if (sistema_memoria->admin_swap->entradas[i].ocupado &&
            sistema_memoria->admin_swap->entradas[i].pid_propietario == pid) {
            
            // Limpiar entrada
            sistema_memoria->admin_swap->entradas[i].ocupado = false;
            sistema_memoria->admin_swap->entradas[i].pid_propietario = -1;
            sistema_memoria->admin_swap->entradas[i].numero_pagina = -1;
            
            // Actualizar contador
            sistema_memoria->admin_swap->paginas_libres_swap++;
            paginas_liberadas++;
        }
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    log_trace(logger, "PID: %d - Liberadas %d páginas de SWAP", pid, paginas_liberadas);
}

bool proceso_tiene_paginas_en_swap(int pid) {
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    for (int i = 0; i < sistema_memoria->admin_swap->cantidad_paginas_swap; i++) {
        if (sistema_memoria->admin_swap->entradas[i].ocupado &&
            sistema_memoria->admin_swap->entradas[i].pid_propietario == pid) {
            pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
            return true;
        }
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    return false;
}

// ============================================================================
// FUNCIONES AUXILIARES PARA GESTIÓN SWAP
// ============================================================================

/**
 * @brief Verifica si un proceso está actualmente suspendido
 * 
 * @param pid PID del proceso a verificar
 * @return true si está suspendido, false en caso contrario
 */
bool proceso_esta_suspendido(int pid) {
    if (!proceso_existe(pid)) {
        return false;
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    return proceso != NULL && proceso->suspendido;
}

/**
 * @brief Obtiene la cantidad de páginas que tiene un proceso en SWAP
 * 
 * @param pid PID del proceso
 * @return Cantidad de páginas en SWAP o 0 si no hay
 */
int obtener_paginas_en_swap(int pid) {
    if (!proceso_esta_suspendido(pid)) {
        return 0;
    }
    
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    int paginas_encontradas = 0;
    
    // Contar páginas del proceso en el array de entradas
    for (int i = 0; i < sistema_memoria->admin_swap->cantidad_paginas_swap; i++) {
        if (sistema_memoria->admin_swap->entradas[i].ocupado &&
            sistema_memoria->admin_swap->entradas[i].pid_propietario == pid) {
            paginas_encontradas++;
        }
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    return paginas_encontradas;
}

/**
 * @brief Obtiene estadísticas del sistema SWAP
 * 
 * @param total_entradas_out Puntero donde almacenar total de entradas SWAP
 * @param espacio_usado_out Puntero donde almacenar espacio usado en bytes
 */
void obtener_estadisticas_swap(int* total_entradas_out, size_t* espacio_usado_out) {
    if (!sistema_memoria || !sistema_memoria->admin_swap) {
        if (total_entradas_out) *total_entradas_out = 0;
        if (espacio_usado_out) *espacio_usado_out = 0;
        return;
    }
    
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    if (total_entradas_out) {
        *total_entradas_out = sistema_memoria->admin_swap->paginas_ocupadas_swap;
    }
    
    if (espacio_usado_out) {
        *espacio_usado_out = sistema_memoria->admin_swap->paginas_ocupadas_swap * cfg->TAM_PAGINA;
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
}

/**
 * @brief Lista todos los procesos que están actualmente en SWAP
 * 
 * @return Lista de PIDs en SWAP (debe ser liberada por el llamador)
 */
t_list* listar_procesos_en_swap(void) {
    t_list* lista_pids = list_create();
    
    if (!sistema_memoria || !sistema_memoria->admin_swap) {
        return lista_pids;
    }
    
    pthread_mutex_lock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Lista temporal para evitar duplicados
    t_list* pids_unicos = list_create();
    
    // Recorrer todas las entradas de SWAP buscando PIDs únicos
    for (int i = 0; i < sistema_memoria->admin_swap->cantidad_paginas_swap; i++) {
        if (sistema_memoria->admin_swap->entradas[i].ocupado) {
            int pid = sistema_memoria->admin_swap->entradas[i].pid_propietario;
            
            // Verificar si el PID ya está en la lista
            bool ya_agregado = false;
            for (int j = 0; j < list_size(pids_unicos); j++) {
                int* pid_existente = list_get(pids_unicos, j);
                if (*pid_existente == pid) {
                    ya_agregado = true;
                    break;
                }
            }
            
            // Si no está, agregarlo
            if (!ya_agregado) {
                int* nuevo_pid = malloc(sizeof(int));
                *nuevo_pid = pid;
                list_add(pids_unicos, nuevo_pid);
            }
        }
    }
    
    pthread_mutex_unlock(&sistema_memoria->admin_swap->mutex_swap);
    
    // Transferir todos los PIDs únicos a la lista final
    for (int i = 0; i < list_size(pids_unicos); i++) {
        int* pid = list_get(pids_unicos, i);
        list_add(lista_pids, pid);
    }
    
    // Limpiar lista temporal (sin liberar los elementos, ya se transfirieron)
    list_clean(pids_unicos);
    list_destroy(pids_unicos);
    
    return lista_pids;
}

// ============================================================================
// FUNCIONES DE UTILIDAD INTERNA
// ============================================================================

/**
 * @brief Aplica el retardo configurado para operaciones de SWAP
 */
void aplicar_retardo_swap(void) {
    if (cfg && cfg->RETARDO_SWAP > 0) {
        usleep(cfg->RETARDO_SWAP * 1000); // Convertir ms a microsegundos
    }
} 