#include "../headers/monitor_memoria.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/interfaz_memoria.h"
#include "../headers/manejo_memoria.h"
#include "../headers/bloqueo_paginas.h"
#include <commons/log.h>
#include <commons/string.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Variables externas
extern t_sistema_memoria* sistema_memoria;
extern t_log* logger;
extern t_config_memoria* cfg;

// Declaraciones de funciones externas
extern t_resultado_memoria liberar_marco(int numero_frame);
extern t_resultado_memoria configurar_entrada_pagina(t_estructura_paginas* estructura, int numero_pagina, int numero_frame);

// ============== FUNCIONES DE VALIDACIÓN Y VERIFICACIÓN ==============

bool verificar_espacio_disponible(int tamanio) {
    // Calcular páginas necesarias
    int paginas_necesarias = (tamanio + cfg->TAM_PAGINA - 1) / cfg->TAM_PAGINA;
    log_debug(logger, "Verificación de espacio - Tamaño: %d bytes, Páginas necesarias: %d", 
             tamanio, paginas_necesarias);
    
    // Verificar espacio disponible de forma thread-safe
    pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);
    bool hay_espacio = sistema_memoria->admin_marcos->frames_libres >= paginas_necesarias;
    pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
    
    return hay_espacio;
}

bool proceso_existe(int pid) {
    if (!sistema_memoria || !sistema_memoria->procesos) {
        log_error(logger, "proceso_existe: Sistema de memoria no inicializado");
        return false;
    }

    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    t_proceso_memoria* proceso = dictionary_get(sistema_memoria->procesos, pid_str);
    
    if (proceso) {
        log_trace(logger, "proceso_existe: Proceso %d encontrado en memoria", pid);
        return true;
    } else {
        log_trace(logger, "proceso_existe: Proceso %d no encontrado en memoria", pid);
        return false;
    }
}

t_proceso_memoria* obtener_proceso(int pid) {
    if (!sistema_memoria || !sistema_memoria->procesos) {
        return NULL;
    }
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    return dictionary_get(sistema_memoria->procesos, pid_str);
}

// ============== FUNCIONES DE CÁLCULO Y UTILIDADES MATEMÁTICAS ==============

void calcular_indices_multinivel(int numero_pagina, int cantidad_niveles, int entradas_por_tabla, int* indices) {
    for (int nivel = 0; nivel < cantidad_niveles; nivel++) {
        int divisor = 1;
        for (int j = 0; j < cantidad_niveles - (nivel + 1); j++)
            divisor *= entradas_por_tabla;
        indices[nivel] = (numero_pagina / divisor) % entradas_por_tabla;
    }
    // Loguear solo los índices válidos
    char indices_str[128] = {0};
    char temp[16];
    for (int i = 0; i < cantidad_niveles; i++) {
        sprintf(temp, "%d", indices[i]);
        strcat(indices_str, temp);
        if (i < cantidad_niveles - 1) strcat(indices_str, ",");
    }
    log_debug(logger, "calcular_indices_multinivel: num_pag=%d indices=[%s]", numero_pagina, indices_str);
}

// ============== FUNCIONES DE LOGGING Y COMUNICACIÓN ==============

void log_instruccion_obtenida(int pid, int pc, t_instruccion* instruccion) {
    char* args_log = string_new();
    if (instruccion->parametros2 && strlen(instruccion->parametros2) > 0) {
        string_append_with_format(&args_log, " %s", instruccion->parametros2);
        if (instruccion->parametros3 && strlen(instruccion->parametros3) > 0) {
            string_append_with_format(&args_log, " %s", instruccion->parametros3);
        }
    }
    log_info(logger, VERDE("## (PID: %d) - Obtener instrucción: %d - Instrucción: %s%s"), 
             pid, pc, instruccion->parametros1, args_log);
    free(args_log);
}

// ============== FUNCIONES DE SERIALIZACIÓN ==============

int serializar_string(void* buffer, int offset, char* string) {
    char* str = string ? string : "";
    int len = strlen(str);
    
    // Escribir tamaño
    memcpy(buffer + offset, &len, sizeof(int));
    offset += sizeof(int);
    
    // Escribir contenido
    memcpy(buffer + offset, str, len);
    offset += len;
    
    return offset;
}

int calcular_tamanio_buffer_instruccion(char* p1, char* p2, char* p3) {
    int len1 = p1 ? strlen(p1) : 0;
    int len2 = p2 ? strlen(p2) : 0;
    int len3 = p3 ? strlen(p3) : 0;
    
    // Formato: [int len1][contenido1][int len2][contenido2][int len3][contenido3]
    return sizeof(int) + len1 + sizeof(int) + len2 + sizeof(int) + len3;
}

void* crear_buffer_instruccion(char* p1, char* p2, char* p3, int* size_out) {
    // Normalizar parámetros (NULL -> string vacío)
    char* param1 = p1 ? p1 : "";
    char* param2 = p2 ? p2 : "";
    char* param3 = p3 ? p3 : "";
    
    // Calcular tamaño total
    int size = calcular_tamanio_buffer_instruccion(param1, param2, param3);
    
    // Crear buffer
    void* buffer = malloc(size);
    if (!buffer) {
        log_error(logger, "Error al asignar memoria para buffer de instrucción");
        *size_out = 0;
        return NULL;
    }
    
    // Serializar los 3 strings
    int offset = 0;
    offset = serializar_string(buffer, offset, param1);
    offset = serializar_string(buffer, offset, param2);
    offset = serializar_string(buffer, offset, param3);
    
    *size_out = size;
    return buffer;
}

void* crear_buffer_error_instruccion(int* size_out) {
    return crear_buffer_instruccion(NULL, NULL, NULL, size_out);
}

// ============== FUNCIONES DE COMUNICACIÓN DE RED ==============

bool enviar_buffer_a_socket(int cliente_socket, void* buffer, int size) {
    // DEBUGGING: Verificar parámetros de entrada
    log_trace(logger, "[ENVIAR_BUFFER] Iniciando envío - Socket: %d, Buffer: %p, Size: %d", cliente_socket, buffer, size);
    
    if (!buffer || size <= 0) {
        log_error(logger, "[ENVIAR_BUFFER] Parámetros inválidos - Buffer: %p, Size: %d", buffer, size);
        return false;
    }
    
    // Enviar tamaño del buffer
    log_trace(logger, "[ENVIAR_BUFFER] Enviando tamaño del buffer: %d bytes", size);
    int bytes_size_enviados = send(cliente_socket, &size, sizeof(int), 0);
    if (bytes_size_enviados <= 0) {
        log_error(logger, "[ENVIAR_BUFFER] Error al enviar tamaño de buffer al socket %d - bytes enviados: %d, errno: %s", 
                  cliente_socket, bytes_size_enviados, strerror(errno));
        return false;
    }
    log_trace(logger, "[ENVIAR_BUFFER] ✓ Tamaño enviado exitosamente - %d bytes", bytes_size_enviados);
    
    // Enviar contenido del buffer
    log_trace(logger, "[ENVIAR_BUFFER] Enviando contenido del buffer: %d bytes", size);
    int bytes_buffer_enviados = send(cliente_socket, buffer, size, 0);
    if (bytes_buffer_enviados <= 0) {
        log_error(logger, "[ENVIAR_BUFFER] Error al enviar buffer al socket %d - bytes enviados: %d, errno: %s", 
                  cliente_socket, bytes_buffer_enviados, strerror(errno));
        return false;
    }
    log_trace(logger, "[ENVIAR_BUFFER] ✓ Buffer enviado exitosamente - %d bytes", bytes_buffer_enviados);
    
    log_trace(logger, "[ENVIAR_BUFFER] ✓ Envío completado exitosamente - Total: %d bytes", 
              bytes_size_enviados + bytes_buffer_enviados);
    
    return true;
}

void procesar_y_enviar_instruccion_valida(int pid, int pc, t_instruccion* instruccion, int cliente_socket) {
    // Log obligatorio
    log_instruccion_obtenida(pid, pc, instruccion);
    
    // Crear buffer serializado
    int size;
    void* buffer = crear_buffer_instruccion(instruccion->parametros1, 
                                           instruccion->parametros2, 
                                           instruccion->parametros3, 
                                           &size);
    if (buffer) {
        // Enviar buffer
        if (!enviar_buffer_a_socket(cliente_socket, buffer, size)) {
            log_error(logger, "Error al enviar instrucción - PID: %d, PC: %d", pid, pc);
        }
        free(buffer);
    } else {
        log_error(logger, "Error al crear buffer para instrucción - PID: %d, PC: %d", pid, pc);
        // Enviar buffer de error como fallback
        int error_size;
        void* error_buffer = crear_buffer_error_instruccion(&error_size);
        if (error_buffer) {
            enviar_buffer_a_socket(cliente_socket, error_buffer, error_size);
            free(error_buffer);
        }
    }
}

void procesar_y_enviar_error_instruccion(int pid, int pc, int cliente_socket) {
    log_error(logger, "No se pudo obtener instrucción - PID: %d, PC: %d", pid, pc);
    
    // Crear y enviar buffer de error
    int size;
    void* buffer = crear_buffer_error_instruccion(&size);
    
    if (buffer) {
        if (!enviar_buffer_a_socket(cliente_socket, buffer, size)) {
            log_error(logger, "Error al enviar buffer de error - PID: %d, PC: %d", pid, pc);
        }
        free(buffer);
    } else {
        log_error(logger, "Error crítico: no se pudo crear buffer de error - PID: %d, PC: %d", pid, pc);
    }
}

// ============== FUNCIONES DE UTILIDADES DE MEMORIA ==============

void aplicar_retardo_memoria(void) {
    usleep(cfg->RETARDO_MEMORIA * 1000);
}

void liberar_instruccion(t_instruccion* instruccion) {
    if (instruccion) {
        if (instruccion->parametros1) free(instruccion->parametros1);
        if (instruccion->parametros2) free(instruccion->parametros2);
        if (instruccion->parametros3) free(instruccion->parametros3);
        free(instruccion);
    }
}

// ============== FUNCIONES DE CREACIÓN DE ESTRUCTURAS ==============

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
    
    log_trace(logger, "Tabla de páginas nivel %d creada con %d entradas", 
              nivel, cfg->ENTRADAS_POR_TABLA);
    
    return tabla;
}

// ============== FUNCIONES DE LECTURA Y ESCRITURA DE MARCOS ==============

bool leer_contenido_marco(int numero_frame, void* buffer) {
    if (!sistema_memoria || !sistema_memoria->memoria_principal) {
        log_error(logger, "leer_contenido_marco: Sistema de memoria no inicializado");
        return false;
    }

    if (numero_frame < 0 || numero_frame >= cfg->TAM_MEMORIA / cfg->TAM_PAGINA) {
        log_error(logger, "leer_contenido_marco: Número de marco inválido %d", numero_frame);
        return false;
    }

    if (!buffer) {
        log_error(logger, "leer_contenido_marco: Buffer nulo");
        return false;
    }

    // Calcular offset en memoria principal
    int offset = numero_frame * cfg->TAM_PAGINA;
    
    // Copiar contenido del marco al buffer
    memcpy(buffer, sistema_memoria->memoria_principal + offset, cfg->TAM_PAGINA);
    
    log_trace(logger, "Marco %d leído exitosamente (%d bytes)", numero_frame, cfg->TAM_PAGINA);
    return true;
}

bool obtener_marcos_proceso(int pid, int* marcos_out, int* cantidad_marcos_out) {
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso || !proceso->estructura_paginas) {
        log_error(logger, "PID: %d - Proceso no encontrado para obtener marcos", pid);
        return false;
    }

    int cantidad_marcos = 0;
    
    // Recorrer todas las páginas del proceso para encontrar marcos asignados
    for (int numero_pagina = 0; numero_pagina < proceso->estructura_paginas->paginas_totales; numero_pagina++) {
        t_entrada_tabla* entrada = buscar_entrada_tabla(proceso->estructura_paginas, numero_pagina);
        if (entrada && entrada->presente) {
            marcos_out[cantidad_marcos] = entrada->numero_frame;
            cantidad_marcos++;
            log_trace(logger, "PID: %d - Página %d -> Marco %d", pid, numero_pagina, entrada->numero_frame);
        }
    }

    *cantidad_marcos_out = cantidad_marcos;
    log_debug(logger, "PID: %d - Encontrados %d marcos en memoria", pid, cantidad_marcos);
    return true;
}

// ============== FUNCIONES DE BÚSQUEDA EN ESTRUCTURAS DE PAGINACIÓN ==============

t_entrada_tabla* buscar_entrada_tabla(t_estructura_paginas* estructura, int numero_pagina) {
    if (!estructura || !estructura->tabla_raiz) {
        return NULL;
    }

    int indices[estructura->cantidad_niveles];
    calcular_indices_multinivel(numero_pagina, estructura->cantidad_niveles, 
                             estructura->entradas_por_tabla, indices);

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

// ============== FUNCIONES DE PROCESAMIENTO DE MEMORIA ==============

t_resultado_memoria procesar_memory_dump(int pid) {
    log_info(logger, VERDE("## (PID: %d) - Memory Dump solicitado"), pid);
    
    // Verificar que el proceso existe
    if (!proceso_existe(pid)) {
        log_error(logger, "PID: %d - Proceso no encontrado para memory dump", pid);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso) {
        log_error(logger, "PID: %d - Error al obtener proceso para memory dump", pid);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    // Crear nombre del archivo dump
    time_t tiempo_actual = time(NULL);
    struct tm* tiempo_local = localtime(&tiempo_actual);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tiempo_local);
    
    char* nombre_archivo = string_from_format("%s%d-%s.dmp", cfg->DUMP_PATH, pid, timestamp);
    
    // Crear archivo dump
    FILE* archivo_dump = fopen(nombre_archivo, "wt");
    if (!archivo_dump) {
        log_error(logger, "PID: %d - Error al crear archivo dump: %s", pid, nombre_archivo);
        free(nombre_archivo);
        return MEMORIA_ERROR_ARCHIVO;
    }
    
    // Obtener marcos del proceso
    int marcos_proceso[64]; // Máximo marcos posibles
    int cantidad_marcos = 0;
    
    if (!obtener_marcos_proceso(pid, marcos_proceso, &cantidad_marcos)) {
        log_error(logger, "PID: %d - Error al obtener marcos del proceso", pid);
        fclose(archivo_dump);
        free(nombre_archivo);
        return MEMORIA_ERROR_LECTURA;
    }
    
    // Escribir contenido de cada marco al archivo
    size_t bytes_escritos_total = 0;
    for (int i = 0; i < cantidad_marcos; i++) {
        int numero_marco = marcos_proceso[i];
        uint32_t direccion_fisica = numero_marco * cfg->TAM_PAGINA;
        
        // Escribir página completa
        size_t bytes_escritos = fwrite(sistema_memoria->memoria_principal + direccion_fisica, 
                                     1, cfg->TAM_PAGINA, archivo_dump);
        
        if (bytes_escritos != cfg->TAM_PAGINA) {
            log_error(logger, "PID: %d - Error al escribir marco %d al dump", pid, numero_marco);
            fclose(archivo_dump);
            free(nombre_archivo);
            return MEMORIA_ERROR_ARCHIVO;
        }
        
        bytes_escritos_total += bytes_escritos;
    }
    fclose(archivo_dump);
    
    // Logs finales
    log_info(logger, "## PID: %d - Memory Dump generado exitosamente", pid);
    log_info(logger, "   - Archivo: %s", nombre_archivo);
    log_info(logger, "   - Tamaño del proceso: %d bytes", proceso->tamanio);
    log_info(logger, "   - Páginas escritas: %d", cantidad_marcos);
    log_info(logger, "   - Bytes totales escritos: %zu", bytes_escritos_total);
    
    free(nombre_archivo);
    return MEMORIA_OK;
}

t_resultado_memoria asignar_marcos_proceso(int pid) {
    t_proceso_memoria* proceso = obtener_proceso(pid);
    if (!proceso || !proceso->estructura_paginas) {
        log_error(logger, "PID: %d - Proceso no encontrado para asignar marcos", pid);
        return MEMORIA_ERROR_PROCESO_NO_ENCONTRADO;
    }
    
    t_estructura_paginas* estructura = proceso->estructura_paginas;
    int marcos_asignados = 0;
    
    log_debug(logger, "PID: %d - Iniciando asignación de %d marcos físicos", pid, estructura->paginas_totales);
    
    // Asignar marcos para cada página del proceso
    for (int numero_pagina = 0; numero_pagina < estructura->paginas_totales; numero_pagina++) {
        int numero_frame = asignar_marco_libre(pid, numero_pagina);
        if (numero_frame == -1) {
            log_error(logger, "PID: %d - No hay marcos libres disponibles para página %d", pid, numero_pagina);
            return MEMORIA_ERROR_NO_ESPACIO;
        }
        
        // Configurar entrada en tabla de páginas
        t_resultado_memoria resultado = configurar_entrada_pagina(estructura, numero_pagina, numero_frame);
        if (resultado != MEMORIA_OK) {
            log_error(logger, "PID: %d - Error al configurar entrada de página %d", pid, numero_pagina);
            liberar_marco(numero_frame);
            return resultado;
        }
        
        marcos_asignados++;
        estructura->paginas_asignadas++;
        
        log_trace(logger, "## Marco asignado - Frame: %d, PID: %d, Página: %d", numero_frame, pid, numero_pagina);
        log_trace(logger, "PID: %d - Página %d asignada al marco %d", pid, numero_pagina, numero_frame);
    }
    
    log_info(logger, "PID: %d - Asignación de marcos completada exitosamente - %d marcos asignados", 
             pid, marcos_asignados);
    
    return MEMORIA_OK;
}

void enviar_instruccion_a_cpu(int fd, int pid, int pc, char* p1, char* p2, char* p3) {
    log_trace(logger, "[PEDIR_INSTRUCCION] Iniciando envío de instrucción a CPU...");

    char* param1 = p1 ? p1 : "";
    char* param2 = p2 ? p2 : "";
    char* param3 = p3 ? p3 : "";

    // Log de los parámetros que se envían
    log_info(logger, COLOR1("[PEDIR_INSTRUCCION]")" Enviando a CPU -> param1: '%s', param2: '%s', param3: '%s'", param1, param2, param3);

    t_paquete* paquete = crear_paquete_op(INSTRUCCION_A_CPU_OP);
    agregar_string_a_paquete(paquete, param1);
    agregar_string_a_paquete(paquete, param2);
    agregar_string_a_paquete(paquete, param3);

    enviar_paquete(paquete, fd);
    eliminar_paquete(paquete);

    log_trace(logger, "[PEDIR_INSTRUCCION] ✓ Instrucción enviada - PID: %d, PC: %d, Socket: %d", pid, pc, fd);
} 