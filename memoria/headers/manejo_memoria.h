#ifndef MANEJO_MEMORIA_H
#define MANEJO_MEMORIA_H

#include <stdint.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#include "estructuras.h"
#include "init_memoria.h"

// Para el checkpoint 2, implementamos algunos mocks

// Inicialización del espacio de memoria
void inicializar_memoria();

// Inicializa las estructuras para manejo de swap
void inicializar_swap();

// Mock para el checkpoint 2: siempre devuelve suficiente memoria
bool hay_espacio_disponible(uint32_t tamanio);

// Mock para reservar memoria para un proceso
void* reservar_memoria(uint32_t pid, uint32_t tamanio);

// Libera la memoria asignada a un proceso
void liberar_memoria(uint32_t pid);

// Actualiza las métricas de un proceso
void actualizar_metricas(uint32_t pid, char* tipo_metrica);

// Lee una página de memoria
void* leer_pagina(uint32_t dir_fisica);

// Escribe una página en memoria
void escribir_pagina(uint32_t dir_fisica, void* datos);

// Funciones para manejo de instrucciones de procesos
t_list* crear_lista_instrucciones();

#endif
