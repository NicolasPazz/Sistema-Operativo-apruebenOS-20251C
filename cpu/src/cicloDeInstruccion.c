#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/funciones.h"
int seguir_ejecutando;
int pid_ejecutando;
int pid_interrupt;
int hay_interrupcion;
int pc;
void ejecutar_ciclo_instruccion() {
    seguir_ejecutando = 1;     
    while(seguir_ejecutando == 1){         
        t_instruccion* instruccion = fetch();
        op_code tipo_instruccion = decode(instruccion->parametros1);
        if(tipo_instruccion != GOTO_OP){
            pc++;
        }    
        execute(tipo_instruccion, instruccion);
        if(seguir_ejecutando){     
            check_interrupt();
        }
    }
}

// fetch
t_instruccion* fetch(){

    t_paquete* paquete = crear_paquete_op(PEDIR_INSTRUCCION_OP);
    agregar_entero_a_paquete(paquete, pc);
    agregar_entero_a_paquete(paquete, pid_ejecutando);         
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    //int codigo = recibir_operacion(fd_memoria);

    t_instruccion* instruccion = recibir_instruccion(fd_memoria);

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
void execute(op_code tipo_instruccion, t_instruccion* instruccion) { //meto las syscalls tambien ??
    switch (tipo_instruccion) {
        case NOOP_OP:
            log_debug(cpu_log, "INSTRUCCION :%d", tipo_instruccion); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_noop();
            break;
        case WRITE_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            //int direccion_logica = atoi(instruccion->parametros1);
            //aca dentro obtenes recien las direcciones
            func_write(instruccion->parametros2, instruccion->parametros3);
            break;
        case READ_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            //int direccion_logica = atoi(instruccion->parametros1);
            //aca dentro obtenes recien las direcciones
            func_read(instruccion->parametros2, instruccion->parametros3);  // warning: passing argument 1 of ‘func_read’ makes integer from pointer without a cast / warning: passing argument 2 of ‘func_read’ makes integer from pointer without a cast
            break;
        case GOTO_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_goto(instruccion->parametros2);
            break;
        case IO_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_io(instruccion->parametros2, instruccion->parametros3); 
            break;
        case INIT_PROC_OP:
            log_debug(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_init_proc(instruccion); // en realidad son 2 parametros
            break;
        case DUMP_MEMORY_OP:
            log_debug(cpu_log, "INSTRUCCION :%d", tipo_instruccion);    // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_dump_memory();
            break;
        case EXIT_OP:
            log_debug(cpu_log, "INSTRUCCION :%d", tipo_instruccion); // warning: format ‘%s’ expects argument of type ‘char *’, but argument 3 has type ‘unsigned int’
            func_exit();
            break;
        default:
            log_error(cpu_log, "Instrucción desconocida\n");
        break;
    }
}

// check interrupt
void check_interrupt() {
    hay_interrupcion = 0;
     //if (pid_ejecutando == pid_interrupt) {        q hacemos con esto
      seguir_ejecutando = 0;   
        t_paquete* paquete_kernel = crear_paquete_op(INTERRUPCION_OP);
        //agregar_entero_a_paquete(paquete_kernel, pid_ejecutando);      
        agregar_entero_a_paquete(paquete_kernel, pc);
        enviar_paquete(paquete_kernel, fd_kernel_interrupt);
        eliminar_paquete(paquete_kernel);
    }
