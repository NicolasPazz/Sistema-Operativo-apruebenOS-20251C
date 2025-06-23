#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/cache.h"
#include "../../utils/headers/sockets.h"

void func_noop() {
    //no hace nada, solo se usa para el log
    log_debug(cpu_log, "PID: %d - Acción: NOOP", pid_ejecutando);
}

//primero se consuta a la cache y despues tlb
void func_write(char* direccion_logica_str, char* datos) {
    int direccion_logica = atoi(direccion_logica_str);
    int tam_pagina = cfg_memoria->TAM_PAGINA;
    int nro_pagina = direccion_logica / tam_pagina;

    // 1. CACHE
    if (cache_habilitada()) {
        int pos = buscar_pagina_en_cache(nro_pagina);
        if (pos != -1) {
            cache_modificar(nro_pagina, datos);
            log_info(cpu_log, "PID: %d - Cache HIT - Pagina: %d - Escritura directa en caché", pid_ejecutando, nro_pagina);
            return;
        }

        // Cache MISS - pedir contenido a Memoria para llenar caché
        int direccion_fisica = traducir_direccion_fisica(direccion_logica);

        t_paquete* paquete = crear_paquete_op(LEER_PAGINA_COMPLETA_OP);
        agregar_entero_a_paquete(paquete, pid_ejecutando);
        int direccion_base = direccion_fisica & ~(cfg_memoria->TAM_PAGINA - 1);
        agregar_entero_a_paquete(paquete, direccion_base);
        enviar_paquete(paquete, fd_memoria);
        eliminar_paquete(paquete);

        int tamanio_buffer;
        char* contenido = recibir_buffer(&tamanio_buffer, fd_memoria);
        cache_escribir(nro_pagina, contenido);
        free(contenido);

        log_info(cpu_log, "PID: %d - Cache MISS - Pagina: %d", pid_ejecutando, nro_pagina);
    }

    //ahora si traducir
    int direccion_fisica = traducir_direccion_fisica(direccion_logica);

    t_paquete* paquete = crear_paquete_op(WRITE_OP);
    //agregar_entero_a_paquete(paquete, pid_ejecutando);
    agregar_entero_con_tamanio_a_paquete(paquete, pid_ejecutando);
    //agregar_entero_a_paquete(paquete, direccion_fisica);
    agregar_entero_con_tamanio_a_paquete(paquete, direccion_fisica);
    agregar_string_a_paquete(paquete, datos);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    log_info(cpu_log, "PID: %d - WRITE - Dir Fisica: %d - Valor: %s", pid_ejecutando, direccion_fisica, datos);
}


void func_read(char* direccion_str, char* tamanio_str) {
    int direccion_logica = atoi(direccion_str);
    int tam_pagina = cfg_memoria->TAM_PAGINA;
    int nro_pagina = direccion_logica / tam_pagina;
    int size = atoi(tamanio_str);

    // 1. CACHE
    if (cache_habilitada()) {
        int pos = buscar_pagina_en_cache(nro_pagina);
        if (pos != -1) {
            char* contenido = cache_leer(nro_pagina); // Asume malloc interno
            log_info(cpu_log, "PID: %d - Cache HIT - Pagina: %d - Valor: %s", pid_ejecutando, nro_pagina, contenido);
            printf("PID: %d - Contenido leído (cache): %s\n", pid_ejecutando, contenido);
            free(contenido);
            return;
        }
        log_info(cpu_log, "PID: %d - Cache MISS - Pagina: %d", pid_ejecutando, nro_pagina);
    }

    // 2. TRADUCCIÓN Y LECTURA EN MEMORIA
    int direccion_fisica = traducir_direccion_fisica(direccion_logica);
    t_paquete *paquete = crear_paquete_op(READ_OP);
    agregar_entero_con_tamanio_a_paquete(paquete, direccion_fisica);
    agregar_entero_con_tamanio_a_paquete(paquete, size);
    agregar_entero_con_tamanio_a_paquete(paquete, pid_ejecutando);
    // agregar_entero_a_paquete(paquete, direccion_fisica);
    // agregar_entero_a_paquete(paquete, size);
    // agregar_entero_a_paquete(paquete, pid_ejecutando);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
    int respuesta_size = 0;
    char* contenido = recibir_buffer(&respuesta_size, fd_memoria);

    log_info(cpu_log, "PID: %d - READ - Dir Fisica: %d - Valor: %s", pid_ejecutando, direccion_fisica, contenido);
    free(contenido);
}

void func_goto(char* valor) {
    pc = atoi(valor);
    
}

void func_io(char* nombre_dispositivo, char* tiempo_str) {
    int tiempo = atoi(tiempo_str);  // Convertir tiempo de string a int
    
    log_info(cpu_log, "[SYSCALL] ▶ Ejecutando IO - Dispositivo: '%s', Tiempo: %d", nombre_dispositivo, tiempo);
    
    t_paquete* paquete = crear_paquete_op(IO_OP);
    agregar_a_paquete(paquete, nombre_dispositivo, strlen(nombre_dispositivo) + 1); // Agregar nombre del dispositivo
    agregar_entero_a_paquete(paquete, tiempo);                                      // Agregar tiempo
    agregar_entero_a_paquete(paquete, pc);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    log_info(cpu_log, "[SYSCALL] ✓ IO enviado a Kernel - Finalizando ejecución del proceso actual");
    seguir_ejecutando = 0;
}

void func_init_proc(t_instruccion* instruccion) {
    char* path = instruccion->parametros2;
    char* size_str = instruccion->parametros3;
    int size = atoi(size_str);

    if (!path || !size_str) {
        log_error(cpu_log, "[SYSCALL] INIT_PROC recibido con parámetros inválidos.");
        return;
    }
    
    log_trace(cpu_log, "[SYSCALL] ▶ Ejecutando INIT_PROC - Archivo: '%s', Tamaño: %d", path, size);
    log_trace(cpu_log, "[SYSCALL] Enviando INIT_PROC_OP a Kernel...");

    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);    // Agrega op code
    agregar_a_paquete(paquete, path, strlen(path)+1);   // Agrega longitud de path y path
    agregar_entero_a_paquete(paquete, size);    // Agrega memory size

    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);
    
    log_trace(cpu_log, "[SYSCALL] ✓ INIT_PROC enviado a Kernel - Continuando ejecución del proceso");

    
}

void func_dump_memory() {
    t_paquete* paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_entero_a_paquete(paquete,pid_ejecutando);
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    log_trace(cpu_log, "[SYSCALL] ✓ DUMP_MEMORY enviado a Kernel - Continuando ejecución del proceso");

    // NO establecer seguir_ejecutando = 0 para continuar con la siguiente instrucción
}

void func_exit() {
    int op = EXIT_OP;
    send(fd_kernel_dispatch, &op, sizeof(int), 0);

    log_trace(cpu_log, "[SYSCALL] ✓ EXIT enviado a Kernel - Finalizando ejecución del proceso");

    seguir_ejecutando = 0; // Solo EXIT debe terminar la ejecución
}

t_instruccion* recibir_instruccion(int conexion) {
    log_trace(cpu_log, "[MEMORIA->CPU] Iniciando recepción de instrucción desde memoria...");
    
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
    
    log_trace(cpu_log, "[MEMORIA->CPU] Buffer recibido exitosamente - Tamaño: %d bytes", size);

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
    log_trace(cpu_log, "[MEMORIA->CPU] ✓ INSTRUCCIÓN RECIBIDA: '%s' | Param2: '%s' | Param3: '%s'", 
              instruccion_nueva->parametros1, 
              instruccion_nueva->parametros2 ? instruccion_nueva->parametros2 : "(vacío)",
              instruccion_nueva->parametros3 ? instruccion_nueva->parametros3 : "(vacío)");

    free(buffer);
    return instruccion_nueva;
}