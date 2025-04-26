#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"

void execute(op_code tipo_instruccion, t_instruccion* instruccion) { //meto las syscalls tambien ??
    switch (tipo_instruccion) {
        case NOOP_OP:
            log_info(cpu_log, "INSTRUCCION :%s", tipo_instruccion);
            func_noop();
            break;
        case WRITE_OP:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3);
            //int direccion_logica = atoi(instruccion->parametros1);
            //aca dentro obtenes recien las direcciones
            func_write(instruccion->parametros2, instruccion->parametros3);
            break;
        case READ_OP:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3);
            //int direccion_logica = atoi(instruccion->parametros1);
            //aca dentro obtenes recien las direcciones
            func_read(instruccion->parametros2, instruccion->parametros3);
            break;
        case GOTO_OP:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2);
            func_goto(instruccion->parametros2);
            break;
        case IO_OP:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2);
            func_io(instruccion->parametros2);
            break;
        case INIT_PROC_OP:
            log_info(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3);
            func_init_proc(instruccion); // en realidad son 2 parametros
            break;
        case DUMP_MEMORY_OP:
            log_info(cpu_log, "INSTRUCCION :%s", tipo_instruccion);
            func_dump_memory();
            break;
        case EXIT_OP:
            log_info(cpu_log, "INSTRUCCION :%s", tipo_instruccion);
            func_exit();
            break;
        default:
            log_info(cpu_log, "Instrucción desconocida\n");
        break;
    }
}