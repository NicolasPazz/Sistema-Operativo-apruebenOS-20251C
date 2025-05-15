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
    uint32_t frame = traducir_direccion(direccion_logica, &desplazamiento, datos);
    t_cache_paginas* cache = inicializar_cache();
    if (cache_habilitada(cache) && (buscar_pagina_en_cache(cache,frame) != -1)){
        cache_modificar(frame, datos);
    } else if (cache_habilitada(cache)) {
        //solicitar_pagina_a_memoria(frame); paquete y pedirle por medio de op code VER
        t_paquete* paquete = crear_paquete_op(PEDIR_PAGINA_OP);
        agregar_entero_a_paquete(paquete, frame);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

        uint32_t pagina = recibir_entero(fd_memoria);     
        cache_escribir(pagina, datos);      
    } else {
        t_paquete* paquete = crear_paquete_op(WRITE_OP);
        agregar_entero_a_paquete(paquete, frame);
        agregar_entero_a_paquete(paquete, desplazamiento);
        agregar_string_a_paquete(paquete, datos, strlen(datos)+1);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

    }

    
}


void func_read(int direccion, int tamanio) {
    uint32_t desplazamiento = 0;
    uint32_t direccion_logica = atoi(direccion);
    uint32_t frame = traducir_direccion(direccion_logica, &desplazamiento);
    // envio el frame a memoria y que devuelva algo?
    // o deberia fijarme en la cache, tlb y despues memoria??

    
}


void func_goto(char* valor) {
    pc = atoi(valor);
    // deberiamos crear un paquete y mandarselo a kernel con este nuevo valor o no es necesario?
}


void func_io(char* nombre_dispositivo, u_int32_t tiempo) {
    t_paquete* paquete = crear_paquete_op(IO_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando); // el proceso es el pid???? debemos enviar el proceso
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
    agregar_string_a_paquete(paquete, path, strlen(path)+1); // tenemos que agregar la funcion agregar_string_a_paquete(a,b,c)
    agregar_entero_a_paquete(paquete, size);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);
    log_info(cpu_log, "PID: %i - INIT_PROC - Archivo: %s - Tamaño: %i", pid_ejecutando, path, size);
    seguir_ejecutando = 0;
} //bien


void func_dump_memory() {
    t_paquete* paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_entero_a_paquete(paquete,pid_ejecutando); //agregue el pid, ver en kernel como dice el enunciado de syscalls.
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);
}


void func_exit() {
    t_paquete* paquete = crear_paquete_op(EXIT_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);
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