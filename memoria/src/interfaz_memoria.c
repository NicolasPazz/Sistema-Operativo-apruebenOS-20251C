#include "../headers/interfaz_memoria.h"
#include <commons/string.h>
#include <string.h>
#include "../headers/init_memoria.h"

// Instanciación de variables globales
t_list* process_instructions_list = NULL;
t_list* processes_in_memory = NULL;

extern t_log* logger;
extern t_config_memoria* cfg;

// Función para inicializar el listado de instrucciones
void instructions_init() {
    if (process_instructions_list == NULL) {
        process_instructions_list = list_create();
        log_debug(logger, "Lista de instrucciones inicializada correctamente");
    }
}

// Función para destruir el listado de instrucciones
void instructions_destroy() {
    if (process_instructions_list != NULL) {
        // Liberar memoria de cada proceso y sus instrucciones
        for (int i = 0; i < list_size(process_instructions_list); i++) {
            t_process_instructions* process_inst = list_get(process_instructions_list, i);
            list_destroy_and_destroy_elements(process_inst->instructions, free);
            free(process_inst);
        }
        list_destroy(process_instructions_list);
        process_instructions_list = NULL;
        log_debug(logger, "Lista de instrucciones destruida correctamente");
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
        log_error(logger, "PID: %d - Error opening instruction file: %s", pid, instructions_file);
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
            // READ <dir>
            instruction->tipo = READ_OP;
            free(instruction->instruccion_base.parametros1);
            instruction->instruccion_base.parametros1 = strdup("READ");
            
            char* param1 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros2);
                instruction->instruccion_base.parametros2 = strdup(param1);
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
            log_warning(logger, "PID: %d - Unknown instruction type: %s", pid, token);
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
    
    log_debug(logger, "PID: %d - Loaded %d instructions from file: %s", 
              pid, line_count, instructions_file);
    
    return process_inst;
}

// Obtiene una instrucción específica para un proceso
t_instruccion* get_instruction(int pid, int pc) {
    if (process_instructions_list == NULL) {
        log_error(logger, "Lista de instrucciones no inicializada");
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
        log_error(logger, "PID: %d - No se encontraron instrucciones para este proceso", pid);
        return NULL;
    }
    
    // Verificar que el PC sea válido
    if (pc >= list_size(process_inst->instructions)) {
        log_error(logger, "PID: %d - PC fuera de rango: %d", pid, pc);
        return NULL;
    }
    
    // Incrementar contador de instrucciones solicitadas para el proceso
    t_proceso_memoria* process_info = get_process_info(pid);
    if (process_info != NULL && process_info->metricas != NULL) {
        process_info->metricas->instrucciones_solicitadas++;
    }
    
    t_extended_instruccion* extended_instr = list_get(process_inst->instructions, pc);
    
    // Loguear la instrucción obtenida (formato requerido)
    char* instr_str = instruction_to_string(extended_instr, pc);
    log_info(logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s", 
             pid, pc, instr_str);
    free(instr_str);
    
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
        log_debug(logger, "Lista de procesos en memoria inicializada correctamente");
    }
}

// Destruye las estructuras de manejo de memoria
void memory_destroy() {
    if (processes_in_memory != NULL) {
        // Liberar memoria de cada proceso
        list_destroy_and_destroy_elements(processes_in_memory, free);
        processes_in_memory = NULL;
        log_debug(logger, "Lista de procesos en memoria destruida correctamente");
    }
}

// Para el checkpoint 2: Devuelve un valor fijo de memoria disponible (mock)
int get_available_memory() {
    // Requisito para Checkpoint 2: Devuelve un valor fijo de espacio libre (mock)
    // En una implementación real, calcularíamos la memoria disponible basada en las asignaciones actuales
    // Para simplificar, asumimos que la mitad de la memoria siempre está disponible
    int memoria_disponible = cfg->TAM_MEMORIA / 2;
    
    log_debug(logger, "Espacio disponible en memoria (mock): %d bytes", memoria_disponible);
    
    return memoria_disponible;
}

// Inicializa un proceso en memoria (mock para checkpoint 2)
int initialize_process(int pid, int size) {
    // Verificar si el proceso ya existe
    for (int i = 0; i < list_size(processes_in_memory); i++) {
        t_proceso_memoria* process = list_get(processes_in_memory, i);
        if (process->pid == pid) {
            log_error(logger, "PID: %d - El proceso ya existe en memoria", pid);
            return -1;
        }
    }
    
    // En el checkpoint 2, siempre aceptamos la inicialización
    t_proceso_memoria* new_process = malloc(sizeof(t_proceso_memoria));
    new_process->pid = pid;
    new_process->tamanio = size;
    new_process->activo = true;
    new_process->suspendido = false;
    new_process->estructura_paginas = NULL;
    new_process->instrucciones = NULL;
    
    // Inicializar métricas
    new_process->metricas = malloc(sizeof(t_metricas_proceso));
    new_process->metricas->pid = pid;
    new_process->metricas->accesos_tabla_paginas = 0;
    new_process->metricas->instrucciones_solicitadas = 0;
    new_process->metricas->bajadas_swap = 0;
    new_process->metricas->subidas_memoria_principal = 0;
    new_process->metricas->lecturas_memoria = 0;
    new_process->metricas->escrituras_memoria = 0;
    pthread_mutex_init(&new_process->metricas->mutex_metricas, NULL);
    
    if (processes_in_memory == NULL) {
        memory_init();
    }
    
    list_add(processes_in_memory, new_process);
    
    // Log obligatorio
    log_info(logger, "## PID: %d - Proceso Creado - Tamaño: %d", pid, size);
    
    return 0; // Éxito
}

// Finaliza un proceso y libera sus recursos
void finalize_process(int pid) {
    if (processes_in_memory == NULL) {
        log_error(logger, "Lista de procesos no inicializada");
        return;
    }
    
    t_proceso_memoria* process_to_remove = NULL;
    int index_to_remove = -1;
    
    // Buscar el proceso por PID
    for (int i = 0; i < list_size(processes_in_memory); i++) {
        t_proceso_memoria* process = list_get(processes_in_memory, i);
        if (process->pid == pid) {
            process_to_remove = process;
            index_to_remove = i;
            break;
        }
    }
    
    if (process_to_remove == NULL) {
        log_error(logger, "PID: %d - No se encontró el proceso a finalizar", pid);
        return;
    }
    
    // Log obligatorio con métricas
    log_info(logger, "## PID: %d - Proceso Destruido - Métricas - Acc.T.Pag: %d; Inst.Sol.: %d; SWAP: %d; Mem.Prin.: %d; Lec.Mem.: %d; Esc.Mem.: %d",
             pid,
             process_to_remove->metricas->accesos_tabla_paginas,
             process_to_remove->metricas->instrucciones_solicitadas,
             process_to_remove->metricas->bajadas_swap,
             process_to_remove->metricas->subidas_memoria_principal,
             process_to_remove->metricas->lecturas_memoria,
             process_to_remove->metricas->escrituras_memoria);
    
    // Liberar métricas
    if (process_to_remove->metricas != NULL) {
        pthread_mutex_destroy(&process_to_remove->metricas->mutex_metricas);
        free(process_to_remove->metricas);
    }
    
    // Eliminar el proceso de la lista
    list_remove_and_destroy_element(processes_in_memory, index_to_remove, free);
    
    // También eliminar sus instrucciones
    for (int i = 0; i < list_size(process_instructions_list); i++) {
        t_process_instructions* p = list_get(process_instructions_list, i);
        if (p->pid == pid) {
            list_remove_and_destroy_element(process_instructions_list, i, (void*)free);
            break;
        }
    }
}

// Obtiene la información de un proceso
t_proceso_memoria* get_process_info(int pid) {
    if (processes_in_memory == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < list_size(processes_in_memory); i++) {
        t_proceso_memoria* process = list_get(processes_in_memory, i);
        if (process->pid == pid) {
            return process;
        }
    }
    
    return NULL;
}
