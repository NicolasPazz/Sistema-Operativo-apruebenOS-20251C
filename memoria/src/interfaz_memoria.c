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
t_process_instructions* load_process_instructions(uint32_t pid, char* instructions_file) {
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
        } 
        else if (strcmp(token, "WRITE") == 0) {
            // WRITE <dir> <valor>
            instruction->tipo = WRITE_OP;
            char* param1 = strtok(NULL, " ");
            char* param2 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros1);
                instruction->instruccion_base.parametros1 = strdup(param1);
            }
            
            if (param2) {
                free(instruction->instruccion_base.parametros2);
                instruction->instruccion_base.parametros2 = strdup(param2);
            }
        } 
        else if (strcmp(token, "READ") == 0) {
            // READ <dir>
            instruction->tipo = READ_OP;
            char* param1 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros1);
                instruction->instruccion_base.parametros1 = strdup(param1);
            }
        } 
        else if (strcmp(token, "GOTO") == 0) {
            // GOTO <dir>
            instruction->tipo = GOTO_OP;
            char* param1 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros1);
                instruction->instruccion_base.parametros1 = strdup(param1);
            }
        } 
        else if (strcmp(token, "IO") == 0) {
            // IO <dispositivo> <tiempo>
            instruction->tipo = IO_OP;
            char* param1 = strtok(NULL, " ");
            char* param2 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros1);
                instruction->instruccion_base.parametros1 = strdup(param1);
            }
            
            if (param2) {
                free(instruction->instruccion_base.parametros2);
                instruction->instruccion_base.parametros2 = strdup(param2);
            }
        } 
        else if (strcmp(token, "INIT_PROC") == 0) {
            // INIT_PROC <nombre_proceso> <tamaño>
            instruction->tipo = INIT_PROC_OP;
            char* param1 = strtok(NULL, " ");
            char* param2 = strtok(NULL, " ");
            
            if (param1) {
                free(instruction->instruccion_base.parametros1);
                instruction->instruccion_base.parametros1 = strdup(param1);
            }
            
            if (param2) {
                free(instruction->instruccion_base.parametros2);
                instruction->instruccion_base.parametros2 = strdup(param2);
            }
        } 
        else if (strcmp(token, "DUMP_MEMORY") == 0) {
            // DUMP_MEMORY has no parameters
            instruction->tipo = DUMP_MEMORY_OP;
        } 
        else if (strcmp(token, "EXIT") == 0) {
            // EXIT has no parameters
            instruction->tipo = EXIT_OP;
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
t_instruccion* get_instruction(uint32_t pid, uint32_t pc) {
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
    t_process_info* process_info = get_process_info(pid);
    if (process_info != NULL) {
        process_info->instructions_requested++;
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
    
    // Usar el tipo de instrucción
    switch (instruction->tipo) {
        case NOOP_OP:
            string_append(&result, "NOOP");
            break;
        case WRITE_OP:
            string_append_with_format(&result, "WRITE %s %s", 
                                     instruction->instruccion_base.parametros1, 
                                     instruction->instruccion_base.parametros2);
            break;
        case READ_OP:
            string_append_with_format(&result, "READ %s", 
                                     instruction->instruccion_base.parametros1);
            break;
        case GOTO_OP:
            string_append_with_format(&result, "GOTO %s", 
                                     instruction->instruccion_base.parametros1);
            break;
        case IO_OP:
            string_append_with_format(&result, "IO %s %s", 
                                     instruction->instruccion_base.parametros1, 
                                     instruction->instruccion_base.parametros2);
            break;
        case INIT_PROC_OP:
            string_append_with_format(&result, "INIT_PROC %s %s", 
                                     instruction->instruccion_base.parametros1, 
                                     instruction->instruccion_base.parametros2);
            break;
        case DUMP_MEMORY_OP:
            string_append(&result, "DUMP_MEMORY");
            break;
        case EXIT_OP:
            string_append(&result, "EXIT");
            break;
        default:
            string_append_with_format(&result, "UNKNOWN (tipo=%d)", instruction->tipo);
            break;
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
uint32_t get_available_memory() {
    // Requisito para Checkpoint 2: Devuelve un valor fijo de espacio libre (mock)
    // En una implementación real, calcularíamos la memoria disponible basada en las asignaciones actuales
    // Para simplificar, asumimos que la mitad de la memoria siempre está disponible
    uint32_t memoria_disponible = cfg->TAM_MEMORIA / 2;
    
    log_debug(logger, "Espacio disponible en memoria (mock): %d bytes", memoria_disponible);
    
    return memoria_disponible;
}

// Inicializa un proceso en memoria (mock para checkpoint 2)
int initialize_process(uint32_t pid, uint32_t size) {
    // Verificar si el proceso ya existe
    for (int i = 0; i < list_size(processes_in_memory); i++) {
        t_process_info* process = list_get(processes_in_memory, i);
        if (process->pid == pid) {
            log_error(logger, "PID: %d - El proceso ya existe en memoria", pid);
            return -1;
        }
    }
    
    // En el checkpoint 2, siempre aceptamos la inicialización
    t_process_info* new_process = malloc(sizeof(t_process_info));
    new_process->pid = pid;
    new_process->size = size;
    new_process->is_active = true;
    
    // Inicializar métricas
    new_process->page_table_accesses = 0;
    new_process->instructions_requested = 0;
    new_process->swap_writes = 0;
    new_process->memory_loads = 0;
    new_process->memory_reads = 0;
    new_process->memory_writes = 0;
    
    if (processes_in_memory == NULL) {
        memory_init();
    }
    
    list_add(processes_in_memory, new_process);
    
    // Log obligatorio
    log_info(logger, "## PID: %d - Proceso Creado - Tamaño: %d", pid, size);
    
    return 0; // Éxito
}

// Finaliza un proceso y libera sus recursos
void finalize_process(uint32_t pid) {
    if (processes_in_memory == NULL) {
        log_error(logger, "Lista de procesos no inicializada");
        return;
    }
    
    t_process_info* process_to_remove = NULL;
    int index_to_remove = -1;
    
    // Buscar el proceso por PID
    for (int i = 0; i < list_size(processes_in_memory); i++) {
        t_process_info* process = list_get(processes_in_memory, i);
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
             process_to_remove->page_table_accesses,
             process_to_remove->instructions_requested,
             process_to_remove->swap_writes,
             process_to_remove->memory_loads,
             process_to_remove->memory_reads,
             process_to_remove->memory_writes);
    
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
t_process_info* get_process_info(uint32_t pid) {
    if (processes_in_memory == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < list_size(processes_in_memory); i++) {
        t_process_info* process = list_get(processes_in_memory, i);
        if (process->pid == pid) {
            return process;
        }
    }
    
    return NULL;
}
