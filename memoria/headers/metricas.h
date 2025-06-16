#ifndef METRICAS_H
#define METRICAS_H

#include "estructuras.h"

// Funciones para incrementar métricas específicas
void incrementar_accesos_tabla_paginas(int pid);
void incrementar_instrucciones_solicitadas(int pid);
void incrementar_bajadas_swap(int pid);
void incrementar_subidas_memoria_principal(int pid);
void incrementar_lecturas_memoria(int pid);
void incrementar_escrituras_memoria(int pid);

// Función para obtener métricas de un proceso
t_metricas_proceso* obtener_metricas_proceso(int pid);

// Función para imprimir métricas al finalizar un proceso
void imprimir_metricas_proceso(int pid);

// Función genérica para actualizar métricas según operación
void actualizar_metricas(int pid, char* operacion);

#endif 