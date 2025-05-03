#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../../headers/funciones/funciones.h"

void execute(op_code tipo_instruccion, t_instruccion* instruccion) { //meto las syscalls tambien ??
    switch (tipo_instruccion) {
        case NOOP_OP:
            log_trace(cpu_log, "INSTRUCCION :%s", tipo_instruccion); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_noop();
            break;
        case WRITE_OP:
            log_trace(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            //int direccion_logica = atoi(instruccion->parametros1);
            //aca dentro obtenes recien las direcciones
            func_write(instruccion->parametros2, instruccion->parametros3);
            break;
        case READ_OP:
            log_trace(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            //int direccion_logica = atoi(instruccion->parametros1);
            //aca dentro obtenes recien las direcciones
            func_read(instruccion->parametros2, instruccion->parametros3);  // warning: passing argument 1 of ‘func_read’ makes integer from pointer without a cast / warning: passing argument 2 of ‘func_read’ makes integer from pointer without a cast
            break;
        case GOTO_OP:
            log_trace(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_goto(instruccion->parametros2);
            break;
        case IO_OP:
            log_trace(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_io(instruccion->parametros2);
            break;
        case INIT_PROC_OP:
            log_trace(cpu_log, "INSTRUCCION :%s - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_init_proc(instruccion); // en realidad son 2 parametros
            break;
        case DUMP_MEMORY_OP:
            log_trace(cpu_log, "INSTRUCCION :%s", tipo_instruccion);    // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_dump_memory();
            break;
        case EXIT_OP:
            log_trace(cpu_log, "INSTRUCCION :%s", tipo_instruccion); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_exit();
            break;
        default:
            log_trace(cpu_log, "Instruccion desconocida\n");
        break;
    }
}