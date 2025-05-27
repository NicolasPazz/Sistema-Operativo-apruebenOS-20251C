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
bool hay_espacio_disponible(int tamanio);

// Mock para reservar memoria para un proceso
void* reservar_memoria(int pid, int tamanio);

// Libera la memoria asignada a un proceso
void liberar_memoria(int pid);

// Actualiza las métricas de un proceso
void actualizar_metricas(int pid, char* tipo_metrica);

// Lee una página de memoria
void* leer_pagina(int dir_fisica);

// Escribe una página en memoria
void escribir_pagina(int dir_fisica, void* datos);

// Funciones para manejo de instrucciones de procesos
t_list* crear_lista_instrucciones();

#endif
