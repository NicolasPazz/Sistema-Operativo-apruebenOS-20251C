#ifndef INIT_MEMORIA_H
#define INIT_MEMORIA_H

#include <stdint.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "estructuras.h"
#include "../../utils/headers/sockets.h"
#include "../../utils/headers/utils.h"

typedef struct {
    int PUERTO_ESCUCHA;
    int TAM_MEMORIA;
    int TAM_PAGINA;
    int ENTRADAS_POR_TABLA;
    int CANTIDAD_NIVELES;
    int RETARDO_MEMORIA;
    char* PATH_SWAPFILE;
    int RETARDO_SWAP;
    char* LOG_LEVEL;
    char* DUMP_PATH;
    char* PATH_INSTRUCCIONES;
} t_config_memoria;

#define MODULENAME "MEMORIA"

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN PRINCIPAL
// ============================================================================

int init(void);                 // inicializa loger, cfg, y semaforos
int cargar_configuracion_memoria(const char *path_cfg);
void iniciar_logger_memoria(void);
void cerrar_programa(void);
int server_escuchar(char*, int);

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN DEL SISTEMA DE MEMORIA
// ============================================================================

// Inicializa el sistema completo de memoria
t_resultado_memoria inicializar_sistema_memoria(void);

// Inicializa la memoria principal (espacio contiguo de usuario)
int inicializar_memoria_principal(void);

// Inicializa la tabla de marcos
int inicializar_tabla_marcos(void);

// Inicializa el sistema de SWAP
int inicializar_sistema_swap(void);

// Inicializa las estructuras de procesos
int inicializar_estructuras_procesos(void);

// ============================================================================
// FUNCIONES DE CREACIÓN DE ESTRUCTURAS
// ============================================================================

// Crea una nueva estructura de paginación para un proceso
t_estructura_paginas* crear_estructura_paginas(int pid, int tamanio_proceso);

// Crea una tabla de páginas de un nivel específico
t_tabla_paginas* crear_tabla_paginas(int nivel);

// Crea las métricas para un proceso
t_metricas_proceso* crear_metricas_proceso(int pid);

// Crea la información completa de un proceso
t_proceso_memoria* crear_proceso_memoria(int pid, int tamanio);

// ============================================================================
// FUNCIONES DE LIBERACIÓN DE MEMORIA
// ============================================================================

// Libera una estructura de paginación
void liberar_estructura_paginas(t_estructura_paginas* estructura);

// Libera una tabla de páginas
void liberar_tabla_paginas(t_tabla_paginas* tabla);

// Libera las métricas de un proceso
void liberar_metricas_proceso(t_metricas_proceso* metricas);

// Libera la información de un proceso
void liberar_proceso_memoria(t_proceso_memoria* proceso);

// Libera todo el sistema de memoria
void liberar_sistema_memoria(void);

// ============================================================================
// FUNCIONES AUXILIARES DE INICIALIZACIÓN
// ============================================================================

// Calcula la cantidad de marcos según la configuración
int calcular_cantidad_marcos(void);

// Calcula la cantidad de páginas de swap
int calcular_cantidad_paginas_swap(void);

// Valida la configuración del sistema
bool validar_configuracion_memoria(void);

// Crea el archivo de swap si no existe
int crear_archivo_swap(void);

// Mapea el archivo de swap en memoria
void* mapear_archivo_swap(void);

void finalizar_mutex(void);

#endif