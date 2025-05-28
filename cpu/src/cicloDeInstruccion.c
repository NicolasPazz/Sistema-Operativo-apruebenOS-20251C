#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/funciones.h"
#include <commons/string.h>

int seguir_ejecutando;
int pid_ejecutando;
int pid_interrupt;
int hay_interrupcion;
int pc;

void ejecutar_ciclo_instruccion() {
    seguir_ejecutando = 1;     
    while(seguir_ejecutando == 1){         
        t_instruccion* instruccion = fetch();
        if (instruccion == NULL) {
            log_error(cpu_log, "Error al obtener instrucción, finalizando ciclo");
            break;
        }
        
        op_code tipo_instruccion = decode(instruccion->parametros1);
        if(tipo_instruccion != GOTO_OP){
            pc++;
        }    
        execute(tipo_instruccion, instruccion);
        
        // Liberar la memoria de la instrucción
        liberar_instruccion(instruccion);
        
        if(seguir_ejecutando){     
            check_interrupt();
        }
    }
}

// fetch
t_instruccion* fetch(){
    log_info(cpu_log, "## PID: %d - FETCH - Program Counter: %d", pid_ejecutando, pc);

    t_paquete* paquete = crear_paquete_op(PEDIR_INSTRUCCION_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);  // CAMBIO: PID primero
    agregar_entero_a_paquete(paquete, pc);              // CAMBIO: PC segundo
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    log_debug(cpu_log, "[FETCH] Solicitando instrucción - PID: %d, PC: %d", pid_ejecutando, pc);

    // Recibir el código de operación primero
    int codigo = recibir_operacion(fd_memoria);
    log_debug(cpu_log, "[FETCH] Código de operación recibido: %d", codigo);

    t_instruccion* instruccion = recibir_instruccion(fd_memoria);
    
    if (instruccion != NULL) {
        log_debug(cpu_log, "[FETCH] Instrucción recibida exitosamente");
    } else {
        log_error(cpu_log, "[FETCH] Error al recibir instrucción");
    }

    return instruccion;
}

// decode
op_code decode(char* nombre_instruccion){
    //INSTRUCCIONES
    if (strcmp(nombre_instruccion, "NOOP") == 0) {
        return NOOP_OP;
    } else if (strcmp(nombre_instruccion, "WRITE") == 0) {
        return WRITE_OP;
    } else if (strcmp(nombre_instruccion, "READ") == 0) {
        return READ_OP;
    } else if (strcmp(nombre_instruccion, "GOTO") == 0) {
        return GOTO_OP;
    //SYSCALLS
    } else if (strcmp(nombre_instruccion, "IO") == 0) {
        return IO_OP;
    } else if (strcmp(nombre_instruccion, "INIT_PROC") == 0) {
        return INIT_PROC_OP;
    } else if (strcmp(nombre_instruccion, "DUMP_MEMORY") == 0) {
        return DUMP_MEMORY_OP;
    } else if (strcmp(nombre_instruccion, "EXIT") == 0) {
        return EXIT_OP;
    }
    return -1; // Código de operación no válido
}

//execute
void execute(op_code tipo_instruccion, t_instruccion* instruccion) {
    // CAMBIO: Log obligatorio 
    char* params_str = string_new();
    if (instruccion->parametros2 && strlen(instruccion->parametros2) > 0) {
        string_append_with_format(&params_str, "%s", instruccion->parametros2);
        if (instruccion->parametros3 && strlen(instruccion->parametros3) > 0) {
            string_append_with_format(&params_str, " %s", instruccion->parametros3);
        }
    }
    
    log_info(cpu_log, "## PID: %d - Ejecutando: %s - %s", 
             pid_ejecutando, 
             instruccion->parametros1, 
             strlen(params_str) > 0 ? params_str : "");
    
    free(params_str);

    switch (tipo_instruccion) {
        case NOOP_OP:
            log_debug(cpu_log, "INSTRUCCION :%d", tipo_instruccion); // warning: format '%s' expects argument of type 'char *', but argument 3 has type 'unsigned int'
            func_noop();
            break;
        case WRITE_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); // warning: format '%s' expects argument of type 'char *', but argument 3 has type 'unsigned int'
            //int direccion_logica = atoi(instruccion->parametros1);
            //aca dentro obtenes recien las direcciones
            func_write(instruccion->parametros2, instruccion->parametros3);
            break;
        case READ_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); // warning: format '%s' expects argument of type 'char *', but argument 3 has type 'unsigned int'
            //int direccion_logica = atoi(instruccion->parametros1);
            //aca dentro obtenes recien las direcciones
            func_read(instruccion->parametros2, instruccion->parametros3);  // warning: passing argument 1 of 'func_read' makes integer from pointer without a cast / warning: passing argument 2 of 'func_read' makes integer from pointer without a cast
            break;
        case GOTO_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2); // warning: format '%s' expects argument of type 'char *', but argument 3 has type 'unsigned int'
            func_goto(instruccion->parametros2);
            break;
        case IO_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2); // warning: format '%s' expects argument of type 'char *', but argument 3 has type 'unsigned int'
            func_io(instruccion->parametros2, instruccion->parametros3); 
            break;
        case INIT_PROC_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); // warning: format '%s' expects argument of type 'char *', but argument 3 has type 'unsigned int'
            func_init_proc(instruccion); // en realidad son 2 parametros
            break;
        case DUMP_MEMORY_OP:
            log_debug(cpu_log, "INSTRUCCION :%d", tipo_instruccion);    // warning: format '%s' expects argument of type 'char *', but argument 3 has type 'unsigned int'
            func_dump_memory();
            break;
        case EXIT_OP:
            log_debug(cpu_log, "INSTRUCCION :%d", tipo_instruccion); // warning: format '%s' expects argument of type 'char *', but argument 3 has type 'unsigned int'
            func_exit();
            break;
        default:
            log_error(cpu_log, "Instrucción desconocida\n");
        break;
    }
}


void check_interrupt(){
    if (hay_interrupcion){
        hay_interrupcion = 0;
        if(pid_ejecutando == pid_interrupt){
            seguir_ejecutando = 0;

            t_paquete* paquete_kernel = crear_paquete_op(INTERRUPCION_OP);
            agregar_entero_a_paquete(paquete_kernel, pid_ejecutando);

            enviar_paquete(paquete_kernel, fd_kernel_interrupt);
            eliminar_paquete(paquete_kernel);
        }
    }
}


// liberamos memoria
void liberar_instruccion(t_instruccion* instruccion) {
    if (instruccion != NULL) {
        if (instruccion->parametros1 != NULL) free(instruccion->parametros1);
        if (instruccion->parametros2 != NULL) free(instruccion->parametros2);
        if (instruccion->parametros3 != NULL) free(instruccion->parametros3);
        free(instruccion);
    }
}
