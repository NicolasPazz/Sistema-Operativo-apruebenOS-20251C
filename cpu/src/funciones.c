#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/cache.h"

void func_noop() {
    sleep(1000); // de donde sacamos el tiempo de ciclo de instruccion
}

void func_write(char* direccion_logica_str, char* datos) {
    uint32_t desplazamiento = 0;
    uint32_t direccion_logica = atoi(direccion_logica_str);
    uint32_t frame = traducir_direccion(direccion_logica, &desplazamiento);
    log_info(cpu_log, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %s", pid_ejecutando, frame, datos); // en el valor de direccion fisica habia otra cosa en el log
    if (cache_habilitada() && (buscar_pagina_en_cache(frame) != -1)){
        cache_modificar(frame, datos);
    } else if (cache_habilitada()) {
        //solicitar_pagina_a_memoria(frame); paquete y pedirle por medio de op code VER
        t_paquete* paquete = crear_paquete_op(PEDIR_PAGINA_OP);
        agregar_entero_a_paquete(paquete, frame);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

        uint32_t pagina = recibir_entero(fd_memoria);     // falta recibir_entero
        cache_escribir(pagina, datos);      
    } else {
        t_paquete* paquete = crear_paquete_op(WRITE_OP);
        agregar_entero_a_paquete(paquete, frame);
        agregar_entero_a_paquete(paquete, desplazamiento);
        agregar_a_paquete(paquete, datos, strlen(datos)+1); //cambie agregar_string_a_paquete por agregar_a_paquete
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);
    }

}


void func_read(char* direccion, char* tamanio) { // ver que deberia hacer tamaño
    uint32_t desplazamiento = 0;
    uint32_t direccion_logica = atoi(direccion);
    uint32_t frame = traducir_direccion(direccion_logica, &desplazamiento);
    log_info(cpu_log,"Pid: %d - Acción: Leer - Dirección Física: %d - Valor: FALTANTE", pid_ejecutando, frame); // FALTA LOS DATOS PARA EL LOG
    t_paquete *paquete = crear_paquete_op(READ_OP); 
    agregar_entero_a_paquete(paquete,frame);
    agregar_entero_a_paquete(paquete,pid_ejecutando);
    enviar_paquete(paquete,fd_memoria);
    eliminar_paquete(paquete);

    
}


void func_goto(char* valor) {
    pc = atoi(valor);
    // deberiamos crear un paquete y mandarselo a kernel con este nuevo valor o no es necesario?
}


void func_io(char* nombre_dispositivo, u_int32_t tiempo) {
    t_paquete* paquete = crear_paquete_op(IO_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);
    agregar_entero_a_paquete(paquete, tiempo);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    seguir_ejecutando = 0;  // aca se bloquea el ciclo, seguir ejec es global 
}


void func_init_proc(t_instruccion* instruccion) {
    char* path = instruccion->parametros2;
    char* size_str = instruccion->parametros3;
    int size = atoi(size_str);

    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
    agregar_a_paquete(paquete, path, strlen(path)+1);
    agregar_entero_a_paquete(paquete, size);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);
    log_info(cpu_log, "PID: %i - INIT_PROC - Archivo: %s - Tamaño: %i", pid_ejecutando, path, size);
    seguir_ejecutando = 0;
} //bien


void func_dump_memory() {
    t_paquete* paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_entero_a_paquete(paquete,pid_ejecutando);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);
}


void func_exit() {
    t_paquete* paquete = crear_paquete_op(EXIT_OP);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    seguir_ejecutando = 0;
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
        log_error(cpu_log, "Instruccion desconocida recibida: %s", instruccion_nueva->parametros1);
    }

    free(buffer);
    return instruccion_nueva;
}