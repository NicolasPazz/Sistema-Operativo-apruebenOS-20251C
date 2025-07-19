#include "../headers/interfaz_memoria.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/metricas.h"
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/memory.h>
#include <commons/collections/list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// Instanciación de variables globales
t_list* process_instructions_list = NULL;
t_list* processes_in_memory = NULL;

extern t_log* logger;
extern t_config_memoria* cfg;

// Función para inicializar el listado de instrucciones
void instructions_init() {
    if (process_instructions_list == NULL) {
        process_instructions_list = list_create();
        log_trace(logger, "Lista de instrucciones inicializada correctamente");
    }
}

void destruir_instruccion_extendida(void* instruccion_ptr) {
    if (!instruccion_ptr) return;
    
    t_extended_instruccion* instruccion = (t_extended_instruccion*)instruccion_ptr;
    
    // Liberar todos los strings creados con strdup()
    if (instruccion->instruccion_base.parametros1) {
        free(instruccion->instruccion_base.parametros1);
    }
    if (instruccion->instruccion_base.parametros2) {
        free(instruccion->instruccion_base.parametros2);
    }
    if (instruccion->instruccion_base.parametros3) {
        free(instruccion->instruccion_base.parametros3);
    }
    
    // Liberar la estructura principal
    free(instruccion);
}

// Función para destruir instrucciones de un proceso específico
void destruir_instrucciones_proceso(int pid) {
    if (process_instructions_list == NULL) {
        return;
    }
    
    // Buscar el proceso por PID
    for (int i = 0; i < list_size(process_instructions_list); i++) {
        t_process_instructions* process_inst = list_get(process_instructions_list, i);
        if (process_inst && process_inst->pid == pid) {
            log_trace(logger, "PID: %d - Liberando %d instrucciones de memoria", 
                     pid, list_size(process_inst->instructions));
            
            // Destruir todas las instrucciones del proceso
            list_destroy_and_destroy_elements(process_inst->instructions, destruir_instruccion_extendida);
            
            // Remover del listado global
            list_remove(process_instructions_list, i);
            free(process_inst);
            
            log_trace(logger, "PID: %d - Instrucciones liberadas correctamente", pid);
            return;
        }
    }
    
    log_debug(logger, "PID: %d - No se encontraron instrucciones para liberar", pid);
}

// Función corregida para destruir TODAS las instrucciones al cerrar sistema
void instructions_destroy() {
    if (process_instructions_list != NULL) {
        // Liberar memoria de cada proceso y sus instrucciones
        for (int i = 0; i < list_size(process_instructions_list); i++) {
            t_process_instructions* process_inst = list_get(process_instructions_list, i);
            
            // Usar función correcta para destruir instrucciones
            list_destroy_and_destroy_elements(process_inst->instructions, destruir_instruccion_extendida);
            free(process_inst);
        }
        list_destroy(process_instructions_list);
        process_instructions_list = NULL;
        log_trace(logger, "Lista de instrucciones destruida correctamente");
    }
}
 
// Para el checkpoint 2, cargamos instrucciones desde un archivo
t_process_instructions* load_process_instructions(int pid, char* instructions_file) {
    t_process_instructions* process_inst = malloc(sizeof(t_process_instructions));
    process_inst->pid = pid;
    process_inst->instructions = list_create();
    
    // Open the instruction file
    FILE* file = fopen(instructions_file, "r");
    if (file == NULL) {
        log_debug(logger, "PID: %d - Error opening instruction file: %s", pid, instructions_file);
        // Create empty instructions list and return (failsafe)
        return process_inst;
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int line_count = 0;
    
    // Read the file line by line
    while ((read = getline(&line, &len, file)) != -1) {
        // Remove newline character if present
        if (line[read-1] == '\n')
            line[read-1] = '\0';
        
        // Skip empty lines
        if (strlen(line) == 0)
            continue;
        
        // Create a new instruction
        t_extended_instruccion* instruction = malloc(sizeof(t_extended_instruccion));
        
        // Initialize all parameters to empty strings
        instruction->instruccion_base.parametros1 = strdup("");
        instruction->instruccion_base.parametros2 = strdup("");
        instruction->instruccion_base.parametros3 = strdup("");
        instruction->tipo = NOOP_OP; // Default to NOOP in case we can't determine the type
        
        // Parse the line to extract instruction and parameters
        char* token = strtok(line, " ");
        if (token == NULL) {
            // Invalid line, skip it
            free(instruction->instruccion_base.parametros1);
            free(instruction->instruccion_base.parametros2);
            free(instruction->instruccion_base.parametros3);
            free(instruction);
            continue;
        }
        
        // Identify instruction type and parse parameters
        if (strcmp(token, "NOOP") == 0) {
            // NOOP has no parameters
            instruction->tipo = NOOP_OP;
            free(instruction->instruccion_base.parametros1);
            instruction->instruccion_base.parametros1 = strdup("NOOP");
        } 
        else if (strcmp(token, "WRITE") == 0) {
            // WRITE <dir> <valor>
            instruction->tipo = WRITE_OP;
            free(instruction->instruccion_base.parametros1);
            instruction->instruccion_base.parametros1 = strdup("WRITE");
            
            char* param1 = strtok(NULL, " ");
            char* param2 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros2);
                instruction->instruccion_base.parametros2 = strdup(param1);
            }
            
            if (param2) {
                free(instruction->instruccion_base.parametros3);
                instruction->instruccion_base.parametros3 = strdup(param2);
            }
        } 
        else if (strcmp(token, "READ") == 0) {
            // READ <dir> <tam>
            instruction->tipo = READ_OP;
            free(instruction->instruccion_base.parametros1);
            instruction->instruccion_base.parametros1 = strdup("READ");
        
            char* param1 = strtok(NULL, " ");
            char* param2 = strtok(NULL, " ");
        
            if (param1) {
                free(instruction->instruccion_base.parametros2);
                instruction->instruccion_base.parametros2 = strdup(param1);
            }
        
            if (param2) {
                free(instruction->instruccion_base.parametros3);
                instruction->instruccion_base.parametros3 = strdup(param2);
            }
        }
        else if (strcmp(token, "GOTO") == 0) {
            // GOTO <dir>
            instruction->tipo = GOTO_OP;
            free(instruction->instruccion_base.parametros1);
            instruction->instruccion_base.parametros1 = strdup("GOTO");
            
            char* param1 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros2);
                instruction->instruccion_base.parametros2 = strdup(param1);
            }
        } 
        else if (strcmp(token, "IO") == 0) {
            // IO <dispositivo> <tiempo>
            instruction->tipo = IO_OP;
            free(instruction->instruccion_base.parametros1);
            instruction->instruccion_base.parametros1 = strdup("IO");
            
            char* param1 = strtok(NULL, " ");
            char* param2 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros2);
                instruction->instruccion_base.parametros2 = strdup(param1);
            }
            
            if (param2) {
                free(instruction->instruccion_base.parametros3);
                instruction->instruccion_base.parametros3 = strdup(param2);
            }
        } 
        else if (strcmp(token, "INIT_PROC") == 0) {
            // INIT_PROC <nombre_proceso> <tamaño>
            instruction->tipo = INIT_PROC_OP;
            free(instruction->instruccion_base.parametros1);
            instruction->instruccion_base.parametros1 = strdup("INIT_PROC");
            
            char* param1 = strtok(NULL, " ");
            char* param2 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros2);
                instruction->instruccion_base.parametros2 = strdup(param1);
            }
            
            if (param2) {
                free(instruction->instruccion_base.parametros3);
                instruction->instruccion_base.parametros3 = strdup(param2);
            }
        } 
        else if (strcmp(token, "DUMP_MEMORY") == 0) {
            // DUMP_MEMORY has no parameters
            instruction->tipo = DUMP_MEMORY_OP;
            free(instruction->instruccion_base.parametros1);
            instruction->instruccion_base.parametros1 = strdup("DUMP_MEMORY");
        } 
        else if (strcmp(token, "EXIT") == 0) {
            // EXIT has no parameters
            instruction->tipo = EXIT_OP;
            free(instruction->instruccion_base.parametros1);
            instruction->instruccion_base.parametros1 = strdup("EXIT");
        } 
        else {
            // Unknown instruction, skip it
            log_debug(logger, "PID: %d - Unknown instruction type: %s", pid, token);
            free(instruction->instruccion_base.parametros1);
            free(instruction->instruccion_base.parametros2);
            free(instruction->instruccion_base.parametros3);
            free(instruction);
            continue;
        }
        
        // Add the instruction to the list
        list_add(process_inst->instructions, instruction);
        line_count++;
    }
    
    fclose(file);
    if (line)
        free(line);
    
    // Add process to the global list
    if (process_instructions_list == NULL) {
        instructions_init();
    }
    list_add(process_instructions_list, process_inst);
    
    log_trace(logger, "PID: %d - Loaded %d instructions from file: %s", 
              pid, line_count, instructions_file);
    
    return process_inst;
}

// Obtiene una instrucción específica para un proceso
t_instruccion* get_instruction(int pid, int pc) {
    if (process_instructions_list == NULL) {
        log_debug(logger, "Lista de instrucciones del proceso PID: %d no inicializada", pid);
        return NULL;
    }
    
    // Buscar el proceso por PID
    t_process_instructions* process_inst = NULL;
    for (int i = 0; i < list_size(process_instructions_list); i++) {
        t_process_instructions* p = list_get(process_instructions_list, i);
        if (p->pid == pid) {
            process_inst = p;
            break;
        }
    }
    
    if (process_inst == NULL) {
        log_debug(logger, "PID: %d - No se encontraron instrucciones para este proceso", pid);
        return NULL;
    }
    
    // Convertir PC de base 1 a base 0 (PC empieza en 1, pero las listas empiezan en 0)
    if (pc == 0) {
        pc = 1; // Si PC era 0 (GOTO 0) el programa rompia daba pc_index -1
    }
    int pc_index = pc - 1;
    
    // Verificar que el PC sea válido
    if (pc_index < 0 || pc_index >= list_size(process_inst->instructions)) {
        log_debug(logger, "PID: %d - PC fuera de rango: %d (índice: %d, tamaño lista: %d)", 
                 pid, pc, pc_index, list_size(process_inst->instructions));
        return NULL;
    }
    
    // Incrementar métrica de instrucciones solicitadas usando la función estándar
    incrementar_instrucciones_solicitadas(pid);
    
    t_extended_instruccion* extended_instr = list_get(process_inst->instructions, pc_index);
    
    // Devolvemos una copia de la instrucción base que será liberada por el llamador
    t_instruccion* result = malloc(sizeof(t_instruccion));
    result->parametros1 = strdup(extended_instr->instruccion_base.parametros1);
    result->parametros2 = strdup(extended_instr->instruccion_base.parametros2);
    result->parametros3 = strdup(extended_instr->instruccion_base.parametros3);
    
    return result;
}

// Convierte una instrucción a string para mostrarla en logs
char* instruction_to_string(t_extended_instruccion* instruction, int pc) {
    char* result = string_new();
    
    // Usar el parametros1 que ahora contiene el nombre de la instrucción
    string_append(&result, instruction->instruccion_base.parametros1);
    
    // Agregar parametros2 si no está vacío
    if (instruction->instruccion_base.parametros2 && strlen(instruction->instruccion_base.parametros2) > 0) {
        string_append_with_format(&result, " %s", instruction->instruccion_base.parametros2);
    }
    
    // Agregar parametros3 si no está vacío  
    if (instruction->instruccion_base.parametros3 && strlen(instruction->instruccion_base.parametros3) > 0) {
        string_append_with_format(&result, " %s", instruction->instruccion_base.parametros3);
    }
    
    return result;
}

// Inicializa las estructuras para manejo de memoria
void memory_init() {
    if (processes_in_memory == NULL) {
        processes_in_memory = list_create();
        log_trace(logger, "Lista de procesos en memoria inicializada correctamente");
    }
}

// Destruye las estructuras de manejo de memoria
void memory_destroy() {
    if (processes_in_memory != NULL) {
        // Liberar memoria de cada proceso
        list_destroy_and_destroy_elements(processes_in_memory, free);
        processes_in_memory = NULL;
        log_trace(logger, "Lista de procesos en memoria destruida correctamente");
    }
}
