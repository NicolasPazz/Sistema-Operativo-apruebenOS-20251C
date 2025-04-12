#include <cicloDeInstruccion.h>
#include "../headers/cpu.h"


void ejecutarCicloInstruccion(int pc, int pid) {
    
    t_instruccion* instruccion = fetch(pc, pid);

    op_code instruccion_nombre = decode(instruccion);

    execute(instruccion_nombre, instruccion);

}


t_instruccion* fetch(int pc, int pid){
    t_instruccion* instruccion = pedir_instruccion_memoria(pc);
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

execute(op_code instruccion_nombre, t_instruccion* instruccion) { //meto las syscalls tambien ??
    switch (instruccion_nombre) {
        case NOOP:
            log_info(cpu_log, "INSTRUCCION :%s", instruccion_nombre);
            funcNoop(instruccion);
            break;
        case WRITE:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", instruccion_nombre, instruccion->parametros2, instruccion->parametros3);
            funcWrite(instruccion);
            break;
        case READ:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", instruccion_nombre, instruccion->parametros2, instruccion->parametros3);
            funcRead(instruccion);
            break;
        case GOTO:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s", instruccion_nombre, instruccion->parametros2);
            funcGoto(instruccion);
            break;
        case IO:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s", instruccion_nombre, instruccion->parametros2);
            funcIO(instruccion);
            break;
        case INIT_PROC:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", instruccion_nombre, instruccion->parametros2, instruccion->parametros3);
            funcInitProc(instruccion);
            break;
        case DUMP_MEMORY:
            log_info(cpu_log, "INSTRUCCION :%s", instruccion_nombre);
            funcDumpMemory(instruccion);
            break;
        case EXIT:
            log_info(cpu_log, "INSTRUCCION :%s", instruccion_nombre);
            funcExit(instruccion);
            break;
        default:
            log_info(cpu_log, "Instrucción desconocida\n");
        break;
    }
}