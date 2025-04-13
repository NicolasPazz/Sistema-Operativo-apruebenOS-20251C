#include <cicloDeInstruccion.h>
#include "../headers/cpu.h"


void ejecutar_ciclo_instruccion(int pc, int pid) {
    
    t_instruccion* instruccion = fetch(pc, pid);

    op_code tipo_instruccion = decode(instruccion);

    execute(tipo_instruccion, instruccion);

}


t_instruccion* fetch(int pc, int pid){
    t_instruccion* instruccion = pedir_instruccion_memoria(pc);
    if (instruccion == NULL)
    {
        log_error("No existe instruccion con el program counter: %d", pc);
        EXIT_FAILURE;
    }
    log_info(cpu_log, "PID: %i - FETCH - Program Counter: %i", pid, pc);
    return instruccion;
}

op_code decode(t_instruccion * instruccion){ // LO HICE CHAR VEMOS SI NOS SIRVE ASI
    //INSTRUCCIONES
    if (strcmp(instruccion->parametros1, "NOOP") == 0) {
        return NOOP;
    } else if (strcmp(instruccion->parametros1, "WRITE") == 0) {
        return WRITE;
    } else if (strcmp(instruccion->parametros1, "READ") == 0) {
        return READ;
    } else if (strcmp(instruccion->parametros1, "GOTO") == 0) {
        return GOTO;
    //SYSCALLS
    } else if (strcmp(instruccion->parametros1, "IO") == 0) {
        return IO;
    } else if (strcmp(instruccion->parametros1, "INIT_PROC") == 0) {
        return INIT_PROC;
    } else if (strcmp(instruccion->parametros1, "DUMP_MEMORY") == 0) {
        return DUMP_MEMORY;
    } else if (strcmp(instruccion->parametros1, "EXIT") == 0) {
        return EXIT;
    }
    return -1; // Código de operación no válido
}

execute(op_code tipo_instruccion, t_instruccion* instruccion) { //meto las syscalls tambien ??
    switch (tipo_instruccion) {
        case NOOP:
            log_info(cpu_log, "INSTRUCCION :%s", tipo_instruccion);
            func_noop(instruccion);
            break;
        case WRITE:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3);
            func_write(instruccion);
            break;
        case READ:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3);
            func_read(instruccion);
            break;
        case GOTO:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2);
            func_goto(instruccion);
            break;
        case IO:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2);
            func_io(instruccion);
            break;
        case INIT_PROC:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3);
            func_init_proc(instruccion);
            break;
        case DUMP_MEMORY:
            log_info(cpu_log, "INSTRUCCION :%s", tipo_instruccion);
            func_dump_memory(instruccion);
            break;
        case EXIT:
            log_info(cpu_log, "INSTRUCCION :%s", tipo_instruccion);
            func_exit(instruccion);
            break;
        default:
            log_info(cpu_log, "Instrucción desconocida\n");
        break;
    }
}

pedir_funcion_memoria(){
    
}