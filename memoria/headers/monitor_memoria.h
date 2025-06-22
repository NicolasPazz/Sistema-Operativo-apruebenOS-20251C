#ifndef MONITOR_MEMORIA_H
#define MONITOR_MEMORIA_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "estructuras.h"

// ============== FUNCIONES DE VALIDACIÓN Y VERIFICACIÓN ==============

/**
 * @brief Verifica si hay espacio disponible en memoria para un tamaño dado
 * @param tamanio Tamaño en bytes a verificar
 * @return true si hay espacio disponible, false en caso contrario
 */
bool verificar_espacio_disponible(int tamanio);

/**
 * @brief Verifica si un proceso existe en el sistema de memoria
 * @param pid PID del proceso a verificar
 * @return true si el proceso existe, false en caso contrario
 */
bool proceso_existe(int pid);

/**
 * @brief Obtiene un proceso de memoria por su PID
 * @param pid PID del proceso
 * @return Puntero al proceso o NULL si no existe
 */
t_proceso_memoria* obtener_proceso(int pid);

// ============== FUNCIONES DE CÁLCULO Y UTILIDADES MATEMÁTICAS ==============

/**
 * @brief Calcula los índices multinivel para una página dada
 * @param numero_pagina Número de página
 * @param cantidad_niveles Cantidad de niveles de paginación
 * @param entradas_por_tabla Entradas por tabla
 * @param indices Array donde se almacenarán los índices calculados
 */
void calcular_indices_multinivel(int numero_pagina, int cantidad_niveles, int entradas_por_tabla, int* indices);

/**
 * @brief Busca una entrada de tabla para una página específica
 * @param estructura Estructura de páginas del proceso
 * @param numero_pagina Número de página a buscar
 * @return Puntero a la entrada de tabla o NULL si no existe
 */
t_entrada_tabla* buscar_entrada_tabla(t_estructura_paginas* estructura, int numero_pagina);

// ============== FUNCIONES DE LOGGING Y COMUNICACIÓN ==============

/**
 * @brief Genera el log obligatorio para obtener instrucción
 * @param pid PID del proceso
 * @param pc Program Counter
 * @param instruccion Instrucción obtenida
 */
void log_instruccion_obtenida(int pid, int pc, t_instruccion* instruccion);

// ============== FUNCIONES DE SERIALIZACIÓN ==============

/**
 * @brief Serializa un string en formato compatible con leer_string
 * @param buffer Buffer donde escribir
 * @param offset Offset actual en el buffer
 * @param string String a serializar (puede ser NULL)
 * @return Nuevo offset después de la serialización
 */
int serializar_string(void* buffer, int offset, char* string);

/**
 * @brief Calcula el tamaño total necesario para serializar 3 strings
 * @param p1 
 * @param p2   
 * @param p3 
 * @return Tamaño total en bytes
 */
int calcular_tamanio_buffer_instruccion(char* p1, char* p2, char* p3);

/**
 * @brief Crea un buffer serializado con 3 strings para enviar a CPU
 * @param p1 
 * @param p2 
 * @param p3 
 * @param size_out Tamaño del buffer creado
 * @return Buffer serializado (debe ser liberado por el caller)
 */
void* crear_buffer_instruccion(char* p1, char* p2, char* p3, int* size_out);

/**
 * @brief Crea un buffer de error con 3 strings vacíos
 * @param size_out Tamaño del buffer creado
 * @return Buffer de error (debe ser liberado por el caller)
 */
void* crear_buffer_error_instruccion(int* size_out);

// ============== FUNCIONES DE COMUNICACIÓN DE RED ==============

/**
 * @brief Envía un buffer serializado a través del socket
 * @param cliente_socket Socket del cliente
 * @param buffer Buffer a enviar
 * @param size Tamaño del buffer
 * @return true si se envió correctamente, false en caso de error
 */
bool enviar_buffer_a_socket(int cliente_socket, void* buffer, int size);

/**
 * @brief Procesa y envía una instrucción válida a la CPU
 * @param pid PID del proceso
 * @param pc Program Counter
 * @param instruccion Instrucción a enviar
 * @param cliente_socket Socket del cliente CPU
 */
void procesar_y_enviar_instruccion_valida(int pid, int pc, t_instruccion* instruccion, int cliente_socket);

/**
 * @brief Procesa y envía un error cuando no se puede obtener la instrucción
 * @param pid PID del proceso
 * @param pc Program Counter
 * @param cliente_socket Socket del cliente CPU
 */
void procesar_y_enviar_error_instruccion(int pid, int pc, int cliente_socket);

// ============== FUNCIONES DE UTILIDADES DE MEMORIA ==============

/**
 * @brief Aplica el retardo configurado para acceso a memoria
 */
void aplicar_retardo_memoria(void);

/**
 * @brief Libera una instrucción y sus parámetros
 * @param instruccion Instrucción a liberar
 */
void liberar_instruccion(t_instruccion* instruccion);

// ============== FUNCIONES DE CREACIÓN DE ESTRUCTURAS ==============

/**
 * @brief Crea una nueva tabla de páginas para un nivel específico
 * @param nivel Nivel de la tabla de páginas
 * @return Puntero a la tabla creada o NULL en caso de error
 */
t_tabla_paginas* crear_tabla_paginas(int nivel);

// ============== FUNCIONES DE LECTURA Y ESCRITURA DE MARCOS ==============

/**
 * @brief Lee el contenido completo de un marco de memoria
 * @param numero_frame Número del marco a leer
 * @param buffer Buffer donde almacenar el contenido leído
 * @return true si se leyó correctamente, false en caso de error
 */
bool leer_contenido_marco(int numero_frame, void* buffer);

/**
 * @brief Obtiene todos los marcos asignados a un proceso
 * @param pid PID del proceso
 * @param marcos_out Array donde almacenar los números de marco
 * @param cantidad_marcos_out Cantidad de marcos encontrados
 * @return true si se obtuvieron correctamente, false en caso de error
 */
bool obtener_marcos_proceso(int pid, int* marcos_out, int* cantidad_marcos_out);

/**
 * @brief Procesa una solicitud de memory dump
 * @param pid PID del proceso
 * @return Resultado de la operación
 */
t_resultado_memoria procesar_memory_dump(int pid);

/**
 * @brief Asigna marcos de memoria para un proceso
 * @param pid PID del proceso
 * @return Resultado de la operación
 */
t_resultado_memoria asignar_marcos_proceso(int pid);

/**
 * @brief Envía una instrucción a la CPU
 * @param pid PID del proceso
 * @param pc Program Counter
 * @param cliente_socket Socket del cliente
 */
void enviar_instruccion_a_cpu(int pid, int pc, int cliente_socket);

#endif // MONITOR_MEMORIA_H 