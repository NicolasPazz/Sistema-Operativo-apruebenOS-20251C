#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"

int seguir_ejecutando;

void ejecutar_ciclo_instruccion(int pc, int pid) {
    seguir_ejecutando = 1;
    while(seguir_ejecutando = 1){

        t_instruccion* instruccion = fetch(pc, pid);

        op_code tipo_instruccion = decode(instruccion->parametros1);

        //una idea despues ver de donde sacar el pc, si pasarlo por parametro o hacerlo global
        if(tipo_instruccion =! GOTO_OP){
            pc++;
        }
    
        execute(tipo_instruccion, instruccion);


        if(seguir_ejecutando){
            check_interrupt();
        }
    }
    

}

/*se deberá chequear si el Kernel nos envió una interrupción al PID que se está ejecutando, 
en caso afirmativo, se devuelve el PID y el Program Counter (PC) actualizado al Kernel 
con motivo de la interrupción. Caso contrario, se descarta la interrupción.
*/
int pid_ejecutando, pid_interrupt, hay_interrupcion, pc; // ver de donde sacar estos, ademas de donde setear el hay interrupcion
 void check_interrupt() {
    hay_interrupcion = 0;
     if (pid_ejecutando == pid_interrupt) {
      seguir_ejecutando = 0;
        t_paquete* paquete_kernel = crear_paquete_op(INTERRUPCION_OP);
        agregar_entero_a_paquete(paquete_kernel, pid_ejecutando);
        agregar_entero_a_paquete(paquete_kernel, pc);
        enviar_paquete(paquete_kernel, fd_kernel_interrupt);
        eliminar_paquete(paquete_kernel);
 }
}


t_instruccion* fetch(int pc, int pid){
    // t_instruccion* instruccion ;//pedir_instruccion_memoria(pc);
    // if (instruccion == NULL)
    // {
    //     log_error("No existe instruccion con el program counter: %d", pc);
    //     EXIT_FAILURE;
    // }
    // log_info(cpu_log, "PID: %i - FETCH - Program Counter: %i", pid, pc);
    // return instruccion;

    t_paquete* paquete = crear_paquete_op(PEDIR_INSTRUCCION_OP);
    agregar_entero_a_paquete(paquete, pc);
    agregar_entero_a_paquete(paquete, pid);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    int codigo = recibir_operacion(fd_memoria);

    t_instruccion* instruccion = recibir_instruccion(fd_memoria);

    return instruccion;
}

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
    return -1; // Código de operación no válido
}

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



void func_noop() {
    //como se cuanto dura el ciclo de instruccion??????????????????
    sleep(1000);
}

void func_write(char* direccion, char* datos) {
    //t_direccion_fisica direccion_fisica = transformar_a_fisica(direccion_logica, 0,10,10); // chequear las ultimas 3 parametros, voy a revisar mañana como hago lo d las entradas
    // hacer el write 
}


void func_read(int direccion, int tamanio) {
    //t_direccion_fisica direccion_fisica = transformar_a_fisica(direccion_logica, 0,10,10); // chequear las ultimas 3 parametros, voy a revisar mañana como hago lo d las entradas
    //hacer el read
}


void func_goto(char* valor) {
    //nuevamente... ver el tema del pc, pid
    pc = atoi(valor);
}


void func_io(char* tiempo_str) { // tiempo no se si seria int ...
    int tiempo = atoi(tiempo_str);
    t_paquete* paquete = crear_paquete_op(IO_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);
    agregar_entero_a_paquete(paquete, tiempo);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    seguir_ejecutando = 0;  // aca se bloquea el ciclo, seguir ejec es global
}


void func_init_proc(t_instruccion* instruccion) {

}


void func_dump_memory() {
    t_paquete* paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
}


void func_exit() {
    t_paquete* paquete = crear_paquete_op(EXIT_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    seguir_ejecutando = 0; // Proceso terminó
}

t_instruccion* recibir_instruccion(int conexion) {
    t_instruccion* instruccion_nueva = malloc(sizeof(t_instruccion));
    int size = 0;
    char* buffer;
    int desp = 0;

    buffer = recibir_buffer(&size, conexion);

    instruccion_nueva->parametros1 = leer_string(buffer, &desp);

    if (strcmp(instruccion_nueva->parametros1, "NOOP") == 0 ||                      //INSTRUCCIONES CON NINGUN PARAMETRO = INSTRUCCION - -
        strcmp(instruccion_nueva->parametros1, "DUMP_MEMORY") == 0 ||
        strcmp(instruccion_nueva->parametros1, "EXIT") == 0) {
    }
    else if (strcmp(instruccion_nueva->parametros1, "GOTO") == 0 ||                 //INSTRUCCIONES CON 1 PARAMETRO = INSTRUCCION - PARAMETRO
             strcmp(instruccion_nueva->parametros1, "IO") == 0) {
        instruccion_nueva->parametros2 = leer_string(buffer, &desp);
    }
    else if (strcmp(instruccion_nueva->parametros1, "WRITE") == 0 ||                //INSTRUCCIONES CON 2 PARAMETROS = INSTRUCCION - PARAMETRO - PARAMETRO
             strcmp(instruccion_nueva->parametros1, "READ") == 0 ||
             strcmp(instruccion_nueva->parametros1, "INIT_PROC") == 0) {
        instruccion_nueva->parametros2 = leer_string(buffer, &desp);
        instruccion_nueva->parametros3 = leer_string(buffer, &desp);
    }
    else {
        log_error(cpu_log, "Instrucción desconocida recibida: %s", instruccion_nueva->parametros1);
    }

    free(buffer);
    return instruccion_nueva;
}