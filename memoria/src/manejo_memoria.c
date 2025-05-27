#include "../headers/manejo_memoria.h"
#include <stdlib.h>
#include <string.h>
#include <commons/string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

extern t_log* logger;
extern t_config_memoria* cfg;
extern void* memoria_principal;
extern void* area_swap;
extern t_list* processes_in_memory;

// Inicialización del espacio de memoria principal
void inicializar_memoria() {
    // Asignar el espacio de memoria principal según la configuración
    memoria_principal = malloc(cfg->TAM_MEMORIA);
    
    if (memoria_principal == NULL) {
        log_error(logger, "Error al inicializar el espacio de memoria principal");
        exit(EXIT_FAILURE);
    }
    
    // Inicializar la memoria con ceros
    memset(memoria_principal, 0, cfg->TAM_MEMORIA);
    
    log_debug(logger, "Espacio de memoria principal inicializado correctamente. Tamaño: %d bytes", cfg->TAM_MEMORIA);
}

// Inicialización de las estructuras para manejo de swap
void inicializar_swap() {
    // Para el checkpoint 2, es suficiente con crear un archivo swap vacío
    int fd = open(cfg->PATH_SWAPFILE, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    
    if (fd == -1) {
        log_error(logger, "Error al crear el archivo SWAP: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Establecer un tamaño inicial para el archivo swap (arbitrario para el checkpoint 2)
    int swap_size = cfg->TAM_MEMORIA * 2; // El doble de la memoria principal
    
    // Extender el archivo al tamaño deseado
    if (ftruncate(fd, swap_size) == -1) {
        log_error(logger, "Error al establecer el tamaño del archivo SWAP: %s", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    // Mapear el archivo a memoria
    area_swap = mmap(NULL, swap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    if (area_swap == MAP_FAILED) {
        log_error(logger, "Error al mapear el archivo SWAP a memoria: %s", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    // Cerrar el descriptor de archivo (el mapeo permanece activo)
    close(fd);
    
    log_debug(logger, "Archivo SWAP inicializado correctamente en %s. Tamaño: %d bytes", cfg->PATH_SWAPFILE, swap_size);
}

// Mock para el checkpoint 2: siempre devuelve suficiente memoria disponible
bool hay_espacio_disponible(int tamanio) {
    // Para el checkpoint 2, siempre devolvemos true
    log_debug(logger, "Verificando espacio disponible (MOCK): %d bytes solicitados", tamanio);
    return true;
}

// Mock para reservar memoria para un proceso
void* reservar_memoria(int pid, int tamanio) {
    // Para el checkpoint 2, no hacemos ninguna asignación real, solo devolvemos
    // la dirección base de la memoria principal como si la hubiéramos asignado
    log_debug(logger, "Reservando memoria para PID %d: %d bytes (MOCK)", pid, tamanio);
    return memoria_principal;
}

// Libera la memoria asignada a un proceso
void liberar_memoria(int pid) {
    // Para el checkpoint 2, no hacemos nada realmente
    log_debug(logger, "Liberando memoria del PID %d (MOCK)", pid);
}

// Actualiza las métricas de un proceso
void actualizar_metricas(int pid, char* tipo_metrica) {
    t_process_info* process = NULL;
    
    // Buscar el proceso en la lista de procesos activos
    for (int i = 0; i < list_size(processes_in_memory); i++) {
        t_process_info* p = list_get(processes_in_memory, i);
        if (p->pid == pid) {
            process = p;
            break;
        }
    }
    
    if (process == NULL) {
        log_error(logger, "No se encontró el proceso %d para actualizar métricas", pid);
        return;
    }
    
    // Actualizar la métrica correspondiente
    if (strcmp(tipo_metrica, "PAGE_TABLE_ACCESS") == 0) {
        process->page_table_accesses++;
    } else if (strcmp(tipo_metrica, "INSTRUCTION_REQUEST") == 0) {
        process->instructions_requested++;
    } else if (strcmp(tipo_metrica, "SWAP_WRITE") == 0) {
        process->swap_writes++;
    } else if (strcmp(tipo_metrica, "MEMORY_LOAD") == 0) {
        process->memory_loads++;
    } else if (strcmp(tipo_metrica, "MEMORY_READ") == 0) {
        process->memory_reads++;
    } else if (strcmp(tipo_metrica, "MEMORY_WRITE") == 0) {
        process->memory_writes++;
    } else {
        log_warning(logger, "Tipo de métrica desconocido: %s", tipo_metrica);
    }
}

// Lee una página de memoria (mock para el checkpoint 2)
void* leer_pagina(int dir_fisica) {
    // Para el checkpoint 2, simplemente devolvemos la dirección en memoria principal
    log_debug(logger, "Leyendo página desde dirección física %d (MOCK)", dir_fisica);
    
    // Simular retardo de memoria
    usleep(cfg->RETARDO_MEMORIA * 1000); // Convertir a microsegundos
    
    // Calcular la dirección real dentro de la memoria principal
    int offset = dir_fisica % cfg->TAM_MEMORIA;
    return memoria_principal + offset;
}

// Escribe una página en memoria (mock para el checkpoint 2)
void escribir_pagina(int dir_fisica, void* datos) {
    log_debug(logger, "Escribiendo página en dirección física %d (MOCK)", dir_fisica);
    
    // Simular retardo de memoria
    usleep(cfg->RETARDO_MEMORIA * 1000); // Convertir a microsegundos
    
    // Calcular la dirección real dentro de la memoria principal
    int offset = dir_fisica % cfg->TAM_MEMORIA;
    
    // Copiar los datos a la memoria principal (asumimos que el tamaño es una página)
    memcpy(memoria_principal + offset, datos, cfg->TAM_PAGINA);
}

// Funciones para manejo de instrucciones de procesos
t_list* crear_lista_instrucciones() {
    return list_create();
}

