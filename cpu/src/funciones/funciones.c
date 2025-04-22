#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/cache.h"

void func_noop() {
    sleep(1000);
}

void func_write(char* direccion_logica_str, char* datos) {
    uint32_t desplazamiento = 0;
    uint32_t direccion_logica = atoi(direccion_logica_str);
    uint32_t frame = traducir_direccion(direccion_logica, &desplazamiento, datos);
    t_cache_paginas* cache = inicializar_cache();
    if (cache_habilitada(cache) && (buscar_pagina_en_cache(cache,frame) != -1)){
        cache_escribir(frame, desplazamiento, datos);
    } else if (cache_habilitada()) {
        //solicitar_pagina_a_memoria(frame); paquete y pedirle por medio de op code VER
        cache_escribir(frame, desplazamiento, datos);
    } else {
        //memoria_escribir(frame, desplazamiento, datos); VER
        sleep(1);
    }

    
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
    char* path = instruccion->parametros2;
    char* size_str = instruccion->parametros3;
    int size = atoi(size_str);

    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
    agregar_string_a_paquete(paquete, path);
    agregar_entero_a_paquete(paquete, size);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    log_info(cpu_log, "PID: %i - INIT_PROC - Archivo: %s - Tamaño: %i", pid_ejecutando, path, size);
    seguir_ejecutando = 0;
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