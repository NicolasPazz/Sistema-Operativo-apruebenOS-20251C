#ifndef INTERFAZ_MEMORIA_H
#define INTERFAZ_MEMORIA_H

#include <stdint.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include <commons/log.h>

#include "estructuras.h"

// Lista global de instrucciones de procesos
extern t_list* process_instructions_list;

// Lista de procesos en memoria
extern t_list* processes_in_memory;

// Funciones para manejo de instrucciones
void instructions_init();
void instructions_destroy();

// Carga las instrucciones de un proceso desde un archivo
t_process_instructions* load_process_instructions(int pid, char* instructions_file);

// Obtiene una instrucción específica de un proceso
t_instruccion* get_instruction(int pid, int pc);

// Convierte una instrucción a string para mostrarla en los logs
// El parámetro pc se mantiene para fines de logging, pero ya no se usa para determinar el tipo
char* instruction_to_string(t_extended_instruccion* instruction, int pc);

// Funciones para manejo de memoria
void memory_init();
void memory_destroy();

// Para el checkpoint 2: Devuelve un valor fijo de espacio libre (mock)
int get_available_memory();

/*
 * FUNCIONES COMENTADAS - DUPLICADAS EN manejo_memoria.c
 * La implementación principal está en manejo_memoria.c
 */

/*
// Inicializa un proceso en memoria (mock para checkpoint 2)
int initialize_process(int pid, int size);

// Finaliza un proceso y libera sus recursos
void finalize_process(int pid);

// Obtiene la información de un proceso
t_proceso_memoria* get_process_info(int pid);
*/

#endif
