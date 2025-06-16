#include "../headers/manejo_memoria.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/metricas.h"
#include "../headers/manejo_swap.h"
#include <commons/log.h>
#include <commons/string.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Variables externas
extern t_sistema_memoria* sistema_memoria;
extern t_log* logger;
extern t_config_memoria* cfg;

// Declaraciones de funciones internas
static void liberar_marcos_proceso(int pid);
static t_resultado_memoria configurar_entrada_pagina(t_estructura_paginas* estructura, int numero_pagina, int numero_frame);
static void calcular_indices_multinivel(int numero_pagina, int cantidad_niveles, int entradas_por_tabla, int* indices);

// ============================================================================
// FUNCIONES DE GESTIÓN DE PROCESOS EN MEMORIA
// ============================================================================

t_resultado_memoria crear_proceso_en_memoria(int pid, int tamanio, char* nombre_archivo) {
    if (pid < 0 || tamanio <= 0) {
        log_error(logger, "PID: %d - Error al crear proceso: Parámetros inválidos", pid);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    if (obtener_proceso(pid)) {
        log_error(logger, "PID: %d - Error al crear proceso: Ya existe", pid);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    t_proceso_memoria* proceso = malloc(sizeof(t_proceso_memoria));
    if (!proceso) {
        log_error(logger, "PID: %d - Error al crear proceso: No hay memoria", pid);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }

    proceso->pid = pid;
    proceso->tamanio = tamanio;
    proceso->nombre_archivo = strdup(nombre_archivo);
    proceso->estructura_paginas = crear_estructura_paginas(pid, tamanio);
    proceso->metricas = crear_metricas_proceso(pid);

    if (!proceso->estructura_paginas || !proceso->metricas) {
        log_error(logger, "PID: %d - Error al crear proceso: Falló la inicialización", pid);
        destruir_proceso(proceso);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }

    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    dictionary_put(sistema_memoria->procesos, pid_str, proceso);
    log_info(logger, "PID: %d - Proceso creado exitosamente", pid);
    return MEMORIA_OK;
}

void liberar_proceso_memoria(t_proceso_memoria* proceso) {
    if (!proceso || !proceso->estructura_paginas) {
        return;
    }

    // Liberar frames asociados al proceso
    liberar_marcos_proceso(proceso->pid);

    // Eliminar proceso del diccionario
    char pid_str[16];
    sprintf(pid_str, "%d", proceso->pid);
    dictionary_remove(sistema_memoria->procesos, pid_str);
    destruir_proceso(proceso);

    log_info(logger, "PID: %d - Proceso eliminado exitosamente", proceso->pid);
}

void destruir_proceso(t_proceso_memoria* proceso) {
    if (!proceso) return;

    if (proceso->estructura_paginas) {
        destruir_tabla_paginas_recursiva(proceso->estructura_paginas->tabla_raiz);
        free(proceso->estructura_paginas);
    }

    if (proceso->metricas) {
        destruir_metricas_proceso(proceso->metricas);
    }

    if (proceso->nombre_archivo) {
        free(proceso->nombre_archivo);
    }

    free(proceso);
}

t_resultado_memoria finalizar_proceso_en_memoria(int pid) {
    if (!sistema_memoria) {
        log_error(logger, "Sistema de memoria no inicializado");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&sistema_memoria->mutex_procesos);
    
    // Verificar si el proceso existe
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    if (!proceso) {
        log_error(logger, "PID: %d - Proceso no encontrado para finalizar", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    // Imprimir métricas antes de finalizar
    imprimir_metricas_proceso(pid);
    
    // Liberar todos los marcos físicos del proceso
    liberar_marcos_proceso(pid);
    
    // Liberar estructuras del proceso
    if (proceso->instrucciones) {
        list_destroy_and_destroy_elements(proceso->instrucciones, free);
    }
    
    if (proceso->estructura_paginas) {
        destruir_estructura_paginas(proceso->estructura_paginas);
    }
    
    if (proceso->metricas) {
        destruir_metricas_proceso(proceso->metricas);
    }
    
    // Actualizar estadísticas del sistema
    sistema_memoria->procesos_activos--;
    sistema_memoria->memoria_utilizada -= proceso->tamanio;
    sistema_memoria->total_liberaciones_memoria++;
    
    // Remover del diccionario y liberar
    dictionary_remove(sistema_memoria->procesos, pid_str);
    dictionary_remove(sistema_memoria->estructuras_paginas, pid_str);
    dictionary_remove(sistema_memoria->metricas_procesos, pid_str);
    dictionary_remove(sistema_memoria->process_instructions, pid_str);
    
    free(proceso);
    
    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
    
    log_info(logger, "## PID: %d - Finaliza el proceso", pid);
    return MEMORIA_OK;
}

t_resultado_memoria suspender_proceso_en_memoria(int pid) {
    if (!sistema_memoria) {
        log_error(logger, "Sistema de memoria no inicializado");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&sistema_memoria->mutex_procesos);
    
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    if (!proceso) {
        log_error(logger, "PID: %d - Proceso no encontrado para suspender", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    if (proceso->suspendido) {
        log_warning(logger, "PID: %d - Proceso ya está suspendido", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_OK;
    }
    
    // Escribir páginas del proceso a SWAP
    t_resultado_memoria resultado = suspender_proceso_completo(pid);
    if (resultado != MEMORIA_OK) {
        log_error(logger, "PID: %d - Error al escribir proceso a SWAP", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return resultado;
    }
    
    // Liberar marcos físicos del proceso
    liberar_marcos_proceso(pid);
    
    // Marcar proceso como suspendido
    proceso->suspendido = true;
    proceso->activo = false;
    
    // Actualizar estadísticas
    sistema_memoria->procesos_activos--;
    sistema_memoria->procesos_suspendidos++;
    sistema_memoria->memoria_utilizada -= proceso->tamanio;
    sistema_memoria->total_suspensiones++;
    
    // Incrementar métrica de bajadas a SWAP
    incrementar_bajadas_swap(pid);
    
    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
    
    log_info(logger, "## PID: %d - Proceso suspendido exitosamente", pid);
    return MEMORIA_OK;
}

t_resultado_memoria reanudar_proceso_en_memoria(int pid) {
    if (!sistema_memoria) {
        log_error(logger, "Sistema de memoria no inicializado");
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    pthread_mutex_lock(&sistema_memoria->mutex_procesos);
    
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    if (!proceso) {
        log_error(logger, "PID: %d - Proceso no encontrado para reanudar", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    if (!proceso->suspendido) {
        log_warning(logger, "PID: %d - Proceso no está suspendido", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_OK;
    }
    
    // Verificar si hay suficientes marcos libres
    int paginas_necesarias = proceso->estructura_paginas->paginas_totales;
    if (obtener_marcos_libres() < paginas_necesarias) {
        log_error(logger, "PID: %d - Memoria insuficiente para reanudar. Necesarias: %d, Disponibles: %d", 
                  pid, paginas_necesarias, obtener_marcos_libres());
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
    }
    
    // Leer proceso desde SWAP
    t_resultado_memoria resultado = reanudar_proceso_suspendido(pid);
    if (resultado != MEMORIA_OK) {
        log_error(logger, "PID: %d - Error al leer proceso desde SWAP", pid);
        pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
        return resultado;
    }
    
    // Asignar nuevos marcos para todas las páginas
    for (int i = 0; i < paginas_necesarias; i++) {
        int numero_frame = asignar_marco_libre(pid, i);
        if (numero_frame == -1) {
            log_error(logger, "PID: %d - Error al asignar marco para página %d", pid, i);
            pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
            return MEMORIA_ERROR_MEMORIA_INSUFICIENTE;
        }
        
        // Actualizar entrada en tabla de páginas
        configurar_entrada_pagina(proceso->estructura_paginas, i, numero_frame);
    }
    
    // Marcar proceso como activo
    proceso->suspendido = false;
    proceso->activo = true;
    
    // Actualizar estadísticas
    sistema_memoria->procesos_activos++;
    sistema_memoria->procesos_suspendidos--;
    sistema_memoria->memoria_utilizada += proceso->tamanio;
    sistema_memoria->total_reanudaciones++;
    
    // Incrementar métrica de subidas a memoria principal
    incrementar_subidas_memoria_principal(pid);
    
    pthread_mutex_unlock(&sistema_memoria->mutex_procesos);
    
    log_info(logger, "## PID: %d - Proceso reanudado exitosamente", pid);
    return MEMORIA_OK;
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE ESTRUCTURAS DE PAGINACIÓN
// ============================================================================

t_estructura_paginas* crear_estructura_paginas(int pid, int tamanio) {
    t_estructura_paginas* estructura = malloc(sizeof(t_estructura_paginas));
    if (!estructura) {
        log_error(logger, "PID: %d - Error al asignar memoria para estructura de paginación", pid);
        return NULL;
    }
    
    // Inicializar estructura
    estructura->pid = pid;
    estructura->cantidad_niveles = cfg->CANTIDAD_NIVELES;
    estructura->entradas_por_tabla = cfg->ENTRADAS_POR_TABLA;
    estructura->tam_pagina = cfg->TAM_PAGINA;
    estructura->tamanio_proceso = tamanio;
    estructura->paginas_totales = (tamanio + cfg->TAM_PAGINA - 1) / cfg->TAM_PAGINA;
    
    // Inicializar mutex
    if (pthread_mutex_init(&estructura->mutex_estructura, NULL) != 0) {
        log_error(logger, "PID: %d - Error al inicializar mutex de estructura", pid);
        free(estructura);
        return NULL;
    }
    
    // Crear tabla raíz
    estructura->tabla_raiz = crear_tabla_paginas(cfg->CANTIDAD_NIVELES - 1);
    if (!estructura->tabla_raiz) {
        log_error(logger, "PID: %d - Error al crear tabla raíz", pid);
        pthread_mutex_destroy(&estructura->mutex_estructura);
        free(estructura);
        return NULL;
    }
    
    log_debug(logger, "PID: %d - Estructura de paginación creada - %d páginas, %d niveles", 
              pid, estructura->paginas_totales, estructura->cantidad_niveles);
    return estructura;
}

t_resultado_memoria configurar_entrada_pagina(t_estructura_paginas* estructura, int numero_pagina, int numero_frame) {
    if (!estructura) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    t_entrada_tabla* entrada = crear_entrada_tabla_si_no_existe(estructura, numero_pagina);
    if (!entrada) {
        log_error(logger, "PID: %d - Error al crear entrada para página %d", estructura->pid, numero_pagina);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    entrada->presente = true;
    entrada->numero_frame = numero_frame;
    entrada->modificado = false;
    entrada->referenciado = true;

    return MEMORIA_OK;
}

// ============================================================================
// FUNCIONES DE ACCESO A TABLAS DE PÁGINAS
// ============================================================================

int obtener_marco_pagina(int pid, int numero_pagina) {
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    t_estructura_paginas* estructura = dictionary_get(sistema_memoria->estructuras_paginas, pid_str);
    if (!estructura) {
        log_error(logger, "PID: %d - Estructura de páginas no encontrada", pid);
        return -1;
    }
    
    pthread_mutex_lock(&estructura->mutex_estructura);
    
    // Incrementar métrica de acceso a tabla de páginas
    incrementar_accesos_tabla_paginas(pid);
    
    // Aplicar retardo de memoria por cada nivel accedido
    for (int i = 0; i < estructura->cantidad_niveles; i++) {
        aplicar_retardo_memoria();
    }
    
    // Calcular índices para navegación
    int indices[estructura->cantidad_niveles];
    int pagina_temp = numero_pagina;
    
    for (int nivel = 0; nivel < estructura->cantidad_niveles; nivel++) {
        indices[nivel] = pagina_temp % estructura->entradas_por_tabla;
        pagina_temp /= estructura->entradas_por_tabla;
    }
    
    // Navegar hasta el nivel hoja
    t_tabla_paginas* tabla_actual = estructura->tabla_raiz;
    
    for (int nivel = estructura->cantidad_niveles - 1; nivel > 0; nivel--) {
        int indice = indices[nivel];
        
        if (!tabla_actual->entradas[indice].presente) {
            log_error(logger, "PID: %d - Página %d no presente en nivel %d", pid, numero_pagina, nivel);
            pthread_mutex_unlock(&estructura->mutex_estructura);
            return -1;
        }
        
        tabla_actual = tabla_actual->entradas[indice].tabla_siguiente;
    }
    
    // Obtener frame del nivel hoja
    int indice_final = indices[0];
    if (!tabla_actual->entradas[indice_final].presente) {
        log_error(logger, "PID: %d - Página %d no presente", pid, numero_pagina);
        pthread_mutex_unlock(&estructura->mutex_estructura);
        return -1;
    }
    
    int numero_frame = tabla_actual->entradas[indice_final].numero_frame;
    
    pthread_mutex_unlock(&estructura->mutex_estructura);
    
    log_trace(logger, "## PID: %d - OBTENER MARCO - Página: %d - Marco: %d", pid, numero_pagina, numero_frame);
    return numero_frame;
}

// ============================================================================
// FUNCIONES DE ACCESO A MEMORIA FÍSICA
// ============================================================================

t_resultado_memoria leer_memoria_fisica(uint32_t direccion_fisica, int tamanio, void* buffer) {
    if (!sistema_memoria || !buffer) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Validar que la dirección física está dentro del rango de memoria
    if (direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "Dirección física fuera de rango: %u", direccion_fisica);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Copiar datos desde la memoria física al buffer
    memcpy(buffer, (char*)sistema_memoria->memoria_principal + direccion_fisica, tamanio);
    
    return MEMORIA_OK;
}

t_resultado_memoria escribir_memoria_fisica(uint32_t direccion_fisica, void* datos, int tamanio) {
    if (!sistema_memoria || !datos) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Validar que la dirección física está dentro del rango de memoria
    if (direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "Dirección física fuera de rango: %u", direccion_fisica);
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Copiar datos desde el buffer a la memoria física
    memcpy((char*)sistema_memoria->memoria_principal + direccion_fisica, datos, tamanio);
    
    return MEMORIA_OK;
}

// ============================================================================
// FUNCIONES DE UTILIDAD
// ============================================================================

static void liberar_marcos_proceso(int pid) {
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso || !proceso->estructura_paginas) {
        return;
    }

    t_estructura_paginas* estructura = proceso->estructura_paginas;
    for (int i = 0; i < estructura->paginas_totales; i++) {
        t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, i);
        if (entrada && entrada->presente) {
            liberar_frame(entrada->numero_frame);
            entrada->presente = false;
            entrada->numero_frame = 0;
        }
    }
}

void aplicar_retardo_memoria(void) {
    if (cfg->RETARDO_MEMORIA > 0) {
        usleep(cfg->RETARDO_MEMORIA * 1000); // Convertir ms a microsegundos
    }
}

void aplicar_retardo_swap(void) {
    if (cfg->RETARDO_SWAP > 0) {
        usleep(cfg->RETARDO_SWAP * 1000); // Convertir ms a microsegundos
    }
}

bool proceso_existe(int pid) {
    if (!sistema_memoria) {
        return false;
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    return dictionary_has_key(sistema_memoria->procesos, pid_str);
}

// ============================================================================
// FUNCIONES DE ACCESO A MEMORIA SEGÚN CONSIGNA
// ============================================================================

/**
 * 1. ACCESO A TABLA DE PÁGINAS
 * El módulo deberá responder con el número de marco correspondiente. 
 * En este evento se deberá tener en cuenta la cantidad de niveles de tablas 
 * de páginas accedido, debiendo considerar un acceso (con su respectivo conteo 
 * de métricas y retardo de acceso) por cada nivel de tabla de páginas accedido.
 */
int acceso_tabla_paginas(int pid, int numero_pagina) {
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso || !proceso->estructura_paginas) {
        return -1;
    }

    t_estructura_paginas* estructura = proceso->estructura_paginas;
    t_entrada_tabla* entrada = buscar_entrada_tabla(estructura, numero_pagina);
    
    if (!entrada || !entrada->presente) {
        return -1;
    }

    return entrada->numero_frame;
}

/**
 * 2. ACCESO A ESPACIO DE USUARIO - LECTURA
 * Ante un pedido de lectura, devolver el valor que se encuentra en la posición pedida.
 */
void* acceso_espacio_usuario_lectura(int pid, int direccion_fisica, int tamanio) {
    log_trace(logger, "PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", 
             pid, direccion_fisica, tamanio);
    
    // Validar parámetros
    if (direccion_fisica < 0 || tamanio <= 0) {
        log_error(logger, "PID: %d - Parámetros inválidos para lectura", pid);
        return NULL;
    }
    
    // Validar que la dirección está dentro del espacio de memoria
    if (direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Dirección fuera de rango: %d + %d > %d", 
                 pid, direccion_fisica, tamanio, cfg->TAM_MEMORIA);
        return NULL;
    }
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para lectura", pid);
        return NULL;
    }
    
    // Actualizar métricas
    char* pid_key = string_itoa(pid);
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_key);
    free(pid_key);
    
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->lecturas_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Reservar memoria para el resultado
    void* resultado = malloc(tamanio);
    if (resultado == NULL) {
        log_error(logger, "PID: %d - Error al reservar memoria para lectura", pid);
        return NULL;
    }
    
    // Copiar datos desde el espacio de usuario
    memcpy(resultado, sistema_memoria->memoria_principal + direccion_fisica, tamanio);
    
    log_info(logger, "## PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", 
             pid, direccion_fisica, tamanio);
    
    return resultado;
}

/**
 * 2. ACCESO A ESPACIO DE USUARIO - ESCRITURA
 * Ante un pedido de escritura, escribir lo indicado en la posición pedida. 
 * En caso satisfactorio se responderá un mensaje de 'OK'.
 */
bool acceso_espacio_usuario_escritura(int pid, int direccion_fisica, int tamanio, void* datos) {
    log_trace(logger, "PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", 
             pid, direccion_fisica, tamanio);
    
    // Validar parámetros
    if (direccion_fisica < 0 || tamanio <= 0 || datos == NULL) {
        log_error(logger, "PID: %d - Parámetros inválidos para escritura", pid);
        return false;
    }
    
    // Validar que la dirección está dentro del espacio de memoria
    if (direccion_fisica + tamanio > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Dirección fuera de rango: %d + %d > %d", 
                 pid, direccion_fisica, tamanio, cfg->TAM_MEMORIA);
        return false;
    }
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para escritura", pid);
        return false;
    }
    
    // Actualizar métricas
    char* pid_key = string_itoa(pid);
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_key);
    free(pid_key);
    
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->escrituras_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Escribir datos en el espacio de usuario
    memcpy(sistema_memoria->memoria_principal + direccion_fisica, datos, tamanio);
    
    log_info(logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", 
             pid, direccion_fisica, tamanio);
    
    return true;
}

/**
 * 3. LEER PÁGINA COMPLETA
 * Se deberá devolver el contenido correspondiente de la página a partir del byte 
 * enviado como dirección física dentro de la Memoria de Usuario, que deberá 
 * coincidir con la posición del byte 0 de la página.
 */
void* leer_pagina_completa(int pid, int direccion_fisica) {
    log_trace(logger, "PID: %d - Leer página completa - Dir. Física: %d", pid, direccion_fisica);
    
    // Validar que la dirección coincide con el byte 0 de una página
    if (direccion_fisica % cfg->TAM_PAGINA != 0) {
        log_error(logger, "PID: %d - Dirección física %d no coincide con byte 0 de página (TAM_PAGINA=%d)", 
                 pid, direccion_fisica, cfg->TAM_PAGINA);
        return NULL;
    }
    
    // Validar que la dirección está dentro del espacio de memoria
    if (direccion_fisica + cfg->TAM_PAGINA > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Página fuera de rango: %d + %d > %d", 
                 pid, direccion_fisica, cfg->TAM_PAGINA, cfg->TAM_MEMORIA);
        return NULL;
    }
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para leer página completa", pid);
        return NULL;
    }
    
    // Actualizar métricas
    char* pid_key = string_itoa(pid);
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_key);
    free(pid_key);
    
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->lecturas_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Reservar memoria para la página completa
    void* pagina_completa = malloc(cfg->TAM_PAGINA);
    if (pagina_completa == NULL) {
        log_error(logger, "PID: %d - Error al reservar memoria para página completa", pid);
        return NULL;
    }
    
    // Copiar página completa desde el espacio de usuario
    memcpy(pagina_completa, sistema_memoria->memoria_principal + direccion_fisica, cfg->TAM_PAGINA);
    
    log_trace(logger, "PID: %d - Página completa leída desde dirección física %d", pid, direccion_fisica);
    
    return pagina_completa;
}

/**
 * 4. ACTUALIZAR PÁGINA COMPLETA
 * Se escribirá la página completa a partir del byte 0 que igual será enviado 
 * como dirección física, esta operación se realizará dentro de la Memoria de 
 * Usuario y se responderá como OK.
 */
bool actualizar_pagina_completa(int pid, int direccion_fisica, void* contenido_pagina) {
    log_trace(logger, "PID: %d - Actualizar página completa - Dir. Física: %d", pid, direccion_fisica);
    
    // Validar parámetros
    if (contenido_pagina == NULL) {
        log_error(logger, "PID: %d - Contenido de página es NULL", pid);
        return false;
    }
    
    // Validar que la dirección coincide con el byte 0 de una página
    if (direccion_fisica % cfg->TAM_PAGINA != 0) {
        log_error(logger, "PID: %d - Dirección física %d no coincide con byte 0 de página (TAM_PAGINA=%d)", 
                 pid, direccion_fisica, cfg->TAM_PAGINA);
        return false;
    }
    
    // Validar que la dirección está dentro del espacio de memoria
    if (direccion_fisica + cfg->TAM_PAGINA > cfg->TAM_MEMORIA) {
        log_error(logger, "PID: %d - Página fuera de rango: %d + %d > %d", 
                 pid, direccion_fisica, cfg->TAM_PAGINA, cfg->TAM_MEMORIA);
        return false;
    }
    
    // Validar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no existe para actualizar página completa", pid);
        return false;
    }
    
    // Actualizar métricas
    char* pid_key = string_itoa(pid);
    t_metricas_proceso* metricas = dictionary_get(sistema_memoria->metricas_procesos, pid_key);
    free(pid_key);
    
    if (metricas != NULL) {
        pthread_mutex_lock(&metricas->mutex_metricas);
        metricas->escrituras_memoria++;
        pthread_mutex_unlock(&metricas->mutex_metricas);
    }
    
    // Aplicar retardo de acceso a memoria
    usleep(cfg->RETARDO_MEMORIA * 1000);
    
    // Escribir página completa en el espacio de usuario
    memcpy(sistema_memoria->memoria_principal + direccion_fisica, contenido_pagina, cfg->TAM_PAGINA);
    
    log_trace(logger, "PID: %d - Página completa actualizada en dirección física %d", pid, direccion_fisica);
    
    return true;
}

// ============================================================================
// FUNCIONES AUXILIARES DE ACCESO A MEMORIA FÍSICA
// ============================================================================

t_resultado_memoria leer_pagina_memoria(int numero_frame, void* buffer) {
    if (!sistema_memoria || !buffer) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Calcular dirección física del frame
    uint32_t direccion_fisica = numero_frame * cfg->TAM_PAGINA;

    // Leer la página completa
    return leer_memoria_fisica(direccion_fisica, cfg->TAM_PAGINA, buffer);
}

t_resultado_memoria escribir_pagina_memoria(int numero_frame, void* contenido) {
    if (!sistema_memoria || !contenido) {
        return MEMORIA_ERROR_DIRECCION_INVALIDA;
    }

    // Calcular dirección física del frame
    uint32_t direccion_fisica = numero_frame * cfg->TAM_PAGINA;

    // Escribir la página completa
    return escribir_memoria_fisica(direccion_fisica, contenido, cfg->TAM_PAGINA);
}

t_entrada_tabla* buscar_entrada_tabla(t_estructura_paginas* estructura, int numero_pagina) {
    if (!estructura || !estructura->tabla_raiz) {
        return NULL;
    }

    int indices[estructura->cantidad_niveles];
    calcular_indices_multinivel(numero_pagina, estructura->entradas_por_tabla, 
                             estructura->cantidad_niveles, indices);

    t_tabla_paginas* tabla_actual = estructura->tabla_raiz;
    t_entrada_tabla* entrada = NULL;

    for (int nivel = 0; nivel < estructura->cantidad_niveles; nivel++) {
        if (!tabla_actual || indices[nivel] >= estructura->entradas_por_tabla) {
            return NULL;
        }

        entrada = &tabla_actual->entradas[indices[nivel]];
        
        if (nivel < estructura->cantidad_niveles - 1) {
            if (!entrada->presente) {
                return NULL;
            }
            tabla_actual = entrada->tabla_siguiente;
        }
    }

    return entrada;
}

t_entrada_tabla* crear_entrada_tabla_si_no_existe(t_estructura_paginas* estructura, int numero_pagina) {
    if (!estructura || !estructura->tabla_raiz) {
        return NULL;
    }

    int indices[estructura->cantidad_niveles];
    calcular_indices_multinivel(numero_pagina, estructura->entradas_por_tabla, 
                             estructura->cantidad_niveles, indices);

    t_tabla_paginas* tabla_actual = estructura->tabla_raiz;
    t_entrada_tabla* entrada = NULL;

    for (int nivel = 0; nivel < estructura->cantidad_niveles; nivel++) {
        if (!tabla_actual || indices[nivel] >= estructura->entradas_por_tabla) {
            return NULL;
        }

        entrada = &tabla_actual->entradas[indices[nivel]];
        
        if (nivel < estructura->cantidad_niveles - 1) {
            if (!entrada->presente) {
                t_tabla_paginas* nueva_tabla = crear_tabla_paginas(nivel + 1);
                if (!nueva_tabla) {
                    return NULL;
                }
                entrada->presente = true;
                entrada->tabla_siguiente = nueva_tabla;
            }
            tabla_actual = entrada->tabla_siguiente;
        }
    }

    return entrada;
}

int asignar_frame_libre(int pid, int numero_pagina) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos) {
        return -1;
    }

    pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);

    if (sistema_memoria->admin_marcos->frames_libres == 0) {
        pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
        return -1;
    }

    // Obtener el primer frame libre de la lista
    int numero_frame = *(int*)list_remove(sistema_memoria->admin_marcos->lista_frames_libres, 0);
    t_frame* frame = &sistema_memoria->admin_marcos->frames[numero_frame];

    // Actualizar el frame
    frame->ocupado = true;
    frame->pid_propietario = pid;
    frame->numero_pagina = numero_pagina;
    frame->timestamp_asignacion = time(NULL);

    // Actualizar contadores
    sistema_memoria->admin_marcos->frames_libres--;
    sistema_memoria->admin_marcos->frames_ocupados++;
    sistema_memoria->admin_marcos->total_asignaciones++;

    // Actualizar bitmap
    bitarray_set_bit(sistema_memoria->admin_marcos->bitmap_frames, numero_frame);

    pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);

    return numero_frame;
}

void liberar_frame(int numero_frame) {
    if (!sistema_memoria || !sistema_memoria->admin_marcos || 
        numero_frame < 0 || numero_frame >= sistema_memoria->admin_marcos->cantidad_total_frames) {
        return;
    }

    pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);

    t_frame* frame = &sistema_memoria->admin_marcos->frames[numero_frame];
    if (!frame->ocupado) {
        pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
        return;
    }

    // Limpiar el frame
    frame->ocupado = false;
    frame->pid_propietario = -1;
    frame->numero_pagina = -1;
    frame->timestamp_asignacion = 0;

    // Actualizar contadores
    sistema_memoria->admin_marcos->frames_libres++;
    sistema_memoria->admin_marcos->frames_ocupados--;
    sistema_memoria->admin_marcos->total_liberaciones++;

    // Actualizar bitmap
    bitarray_clean_bit(sistema_memoria->admin_marcos->bitmap_frames, numero_frame);

    // Agregar a la lista de frames libres
    int* numero_frame_ptr = malloc(sizeof(int));
    *numero_frame_ptr = numero_frame;
    list_add(sistema_memoria->admin_marcos->lista_frames_libres, numero_frame_ptr);

    pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
}

t_proceso_memoria* obtener_proceso(int pid) {
    if (!sistema_memoria || !sistema_memoria->procesos) {
        return NULL;
    }
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    return dictionary_get(sistema_memoria->procesos, pid_str);
}

t_metricas_proceso* crear_metricas_proceso(int pid) {
    t_metricas_proceso* metricas = malloc(sizeof(t_metricas_proceso));
    if (!metricas) {
        log_error(logger, "Error al crear métricas para proceso %d", pid);
        return NULL;
    }

    metricas->pid = pid;
    metricas->accesos_tabla_paginas = 0;
    metricas->instrucciones_solicitadas = 0;
    metricas->bajadas_swap = 0;
    metricas->subidas_memoria_principal = 0;
    metricas->lecturas_memoria = 0;
    metricas->escrituras_memoria = 0;
    metricas->timestamp_creacion = time(NULL);
    metricas->timestamp_ultimo_acceso = time(NULL);

    pthread_mutex_init(&metricas->mutex_metricas, NULL);

    return metricas;
}

static void calcular_indices_multinivel(int numero_pagina, int cantidad_niveles, int entradas_por_tabla, int* indices) {
    int pagina_temp = numero_pagina;
    for (int nivel = 0; nivel < cantidad_niveles; nivel++) {
        indices[nivel] = pagina_temp % entradas_por_tabla;
        pagina_temp /= entradas_por_tabla;
    }
}

t_tabla_paginas* crear_tabla_paginas(int nivel) {
    t_tabla_paginas* tabla = malloc(sizeof(t_tabla_paginas));
    if (!tabla) {
        log_error(logger, "Error al crear tabla de páginas nivel %d", nivel);
        return NULL;
    }
    
    tabla->nivel = nivel;
    tabla->entradas = calloc(cfg->ENTRADAS_POR_TABLA, sizeof(t_entrada_tabla));
    
    if (!tabla->entradas) {
        log_error(logger, "Error al asignar memoria para entradas de tabla nivel %d", nivel);
        free(tabla);
        return NULL;
    }
    
    return tabla;
}

void* leer_pagina(int dir_fisica) {
    if (!sistema_memoria || !sistema_memoria->memoria_principal) {
        return NULL;
    }
    
    // Validar que la dirección esté dentro del rango de memoria
    if (dir_fisica < 0 || dir_fisica + cfg->TAM_PAGINA > cfg->TAM_MEMORIA) {
        log_error(logger, "Dirección física %d fuera de rango", dir_fisica);
        return NULL;
    }
    
    // Asignar memoria para la página
    void* pagina = malloc(cfg->TAM_PAGINA);
    if (!pagina) {
        log_error(logger, "Error al asignar memoria para página en dirección %d", dir_fisica);
        return NULL;
    }
    
    // Copiar el contenido de la página
    memcpy(pagina, sistema_memoria->memoria_principal + dir_fisica, cfg->TAM_PAGINA);
    
    return pagina;
} 