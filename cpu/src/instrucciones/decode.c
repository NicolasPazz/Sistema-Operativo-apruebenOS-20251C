#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"


op_code decode(char* nombre_instruccion){ // LO HICE CHAR VEMOS SI NOS SIRVE ASI
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
    return -1; // Codigo de operacion no valido
}
