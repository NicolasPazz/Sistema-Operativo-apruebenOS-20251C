#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/cache.h"
#include "../../utils/headers/sockets.h"

void func_noop() {
    //no hace nada, solo se usa para el log
    log_info(cpu_log, "PID: %d - Acción: NOOP", pid_ejecutando);
}

void func_write(char* direccion_logica_str, char* datos) {
    int desplazamiento = 0;
    int direccion_logica = atoi(direccion_logica_str);
    int frame = traducir_direccion(direccion_logica, &desplazamiento);
    log_info(cpu_log, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %s", pid_ejecutando, frame, datos); 
    if (cache_habilitada() && (buscar_pagina_en_cache(frame) != -1)){
        cache_modificar(frame, datos);
    } else if (cache_habilitada()) {
        //solicitar_pagina_a_memoria(frame); paquete y pedirle por medio de op code VER
        t_paquete* paquete = crear_paquete_op(PEDIR_PAGINA_OP);
        agregar_entero_a_paquete(paquete, frame);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

        int pagina;
        recibir_entero(fd_memoria, &pagina);
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

void func_read(char* direccion, char* tamanio) {
    int desplazamiento = 0;
    int size = atoi(tamanio);
    int direccion_logica = atoi(direccion);

    int frame = traducir_direccion(direccion_logica, &desplazamiento);
    int direccion_fisica = frame * cfg_memoria->TAM_PAGINA + desplazamiento;

    t_paquete *paquete = crear_paquete_op(READ_OP);
    agregar_entero_a_paquete(paquete, direccion_fisica);
    agregar_entero_a_paquete(paquete, size);
    agregar_entero_a_paquete(paquete, pid_ejecutando);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    // Recibir el buffer
    int respuesta_size = 0;
    char* contenido = recibir_buffer(&respuesta_size, fd_memoria);

    log_info(cpu_log, 
        "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %s", 
        pid_ejecutando, direccion_fisica, contenido);

    printf("PID: %d - Contenido leído: %s\n", pid_ejecutando, contenido);
    free(contenido);
}



void func_goto(char* valor) {
    pc = atoi(valor);
    // deberiamos crear un paquete y mandarselo a kernel con este nuevo valor o no es necesario?
}


void func_io(char* nombre_dispositivo, char* tiempo_str) {
    int tiempo = atoi(tiempo_str);  // Convertir tiempo de string a int
    
    log_info(cpu_log, "[SYSCALL] ▶ Ejecutando IO - Dispositivo: '%s', Tiempo: %d", nombre_dispositivo, tiempo);
    
    t_paquete* paquete = crear_paquete_op(IO_OP);
    agregar_a_paquete(paquete, nombre_dispositivo, strlen(nombre_dispositivo) + 1); // Agregar nombre del dispositivo
    agregar_entero_a_paquete(paquete, tiempo);                                      // Agregar tiempo
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    log_info(cpu_log, "[SYSCALL] ✓ IO enviado a Kernel - Finalizando ejecución del proceso actual");
    seguir_ejecutando = 0;
}


void func_init_proc(t_instruccion* instruccion) {
    char* path = instruccion->parametros2;
    char* size_str = instruccion->parametros3;
    int size = atoi(size_str);

    log_info(cpu_log, "[SYSCALL] ▶ Ejecutando INIT_PROC - Archivo: '%s', Tamaño: %d", path, size);
    log_debug(cpu_log, "[SYSCALL] Enviando INIT_PROC_OP a Kernel...");

    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
    agregar_a_paquete(paquete, path, strlen(path)+1);
    agregar_entero_a_paquete(paquete, size);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);
    
    //log_info(cpu_log, "## (<PID>) - Solicitó syscall: INIT_PROC");
    log_info(cpu_log, "[SYSCALL] ✓ INIT_PROC enviado a Kernel - Finalizando ejecución del proceso actual");

    seguir_ejecutando = 0;
}


void func_dump_memory() {
    t_paquete* paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_entero_a_paquete(paquete,pid_ejecutando);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    seguir_ejecutando = 0;
}


void func_exit() {
    t_paquete* paquete = crear_paquete_op(EXIT_OP);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    seguir_ejecutando = 0;
}

t_instruccion* recibir_instruccion(int conexion) {
    log_debug(cpu_log, "[MEMORIA->CPU] Iniciando recepción de instrucción desde memoria...");
    
    t_instruccion* instruccion_nueva = malloc(sizeof(t_instruccion));
    int size = 0;
    char* buffer;
    int desp = 0;

    // Recibir el buffer del paquete
    buffer = recibir_buffer(&size, conexion);
    
    if (buffer == NULL || size <= 0) {
        log_error(cpu_log, "[MEMORIA->CPU] Error al recibir buffer de instrucción - Buffer: %p, Size: %d", buffer, size);
        free(instruccion_nueva);
        return NULL;
    }
    
    log_debug(cpu_log, "[MEMORIA->CPU] Buffer recibido exitosamente - Tamaño: %d bytes", size);

    // Leer los 3 parámetros en orden (siempre están presentes)
    instruccion_nueva->parametros1 = leer_string(buffer, &desp);
    instruccion_nueva->parametros2 = leer_string(buffer, &desp);  
    instruccion_nueva->parametros3 = leer_string(buffer, &desp);

    // Verificar que la lectura fue exitosa
    if (instruccion_nueva->parametros1 == NULL) {
        log_error(cpu_log, "[MEMORIA->CPU] Error al leer parámetros de instrucción");
        free(instruccion_nueva);
        free(buffer);
        return NULL;
    }

    // Log detallado de la instrucción recibida
    log_info(cpu_log, "[MEMORIA->CPU] ✓ INSTRUCCIÓN RECIBIDA: '%s' | Param2: '%s' | Param3: '%s'", 
              instruccion_nueva->parametros1, 
              instruccion_nueva->parametros2 ? instruccion_nueva->parametros2 : "(vacío)",
              instruccion_nueva->parametros3 ? instruccion_nueva->parametros3 : "(vacío)");

    free(buffer);
    return instruccion_nueva;
}