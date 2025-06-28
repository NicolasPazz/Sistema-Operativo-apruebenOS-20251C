#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/cache.h"
#include "../../utils/headers/sockets.h"
#include "../headers/main.h"

void func_noop() {
    //No hace nada, solo se usa para el log
    log_debug(cpu_log, "## PID: %d - Acción: NOOP", pid_ejecutando);
}

// Primero se consuta a la cache y despues tlb
void func_write(char* direccion_logica_str, char* datos) {
    int direccion_logica = atoi(direccion_logica_str);
    int tam_pagina = cfg_memoria->TAM_PAGINA;
    int nro_pagina = direccion_logica / tam_pagina;

    // 1. CACHE
    if (cache_habilitada()) {
        int pos = buscar_pagina_en_cache(nro_pagina);
        if (pos != -1) {
            cache_modificar(nro_pagina, datos);
            log_info(cpu_log, VERDE("PID: %d - Cache HIT - Pagina: %d - Escritura directa en cache"), pid_ejecutando, nro_pagina);
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

        log_info(cpu_log, VERDE("PID: %d - Cache MISS - Pagina: %d"), pid_ejecutando, nro_pagina);
    }

    //ahora si traducir
    int direccion_fisica = traducir_direccion_fisica(direccion_logica);

    t_paquete* paquete = crear_paquete_op(WRITE_OP);
    // Usar agregar_a_paquete para consistencia con recibir_contenido_paquete
    agregar_a_paquete(paquete, &pid_ejecutando, sizeof(int));
    agregar_a_paquete(paquete, &direccion_fisica, sizeof(int));
    agregar_a_paquete(paquete, datos, strlen(datos) + 1);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    log_info(cpu_log, VERDE("PID: %d - WRITE - Dir Fisica: %d - Valor: %s"), pid_ejecutando, direccion_fisica, datos);
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
            log_info(cpu_log, VERDE("PID: %d - Cache HIT - Pagina: %d - Valor: %s"), pid_ejecutando, nro_pagina, contenido);
            printf("PID: %d - Contenido leído (cache): %s\n", pid_ejecutando, contenido);
            free(contenido);
            return;
        }
        log_info(cpu_log, VERDE("PID: %d - Cache MISS - Pagina: %d"), pid_ejecutando, nro_pagina);
    }

    // 2. TRADUCCIÓN Y LECTURA EN MEMORIA
    int direccion_fisica = traducir_direccion_fisica(direccion_logica);
    t_paquete *paquete = crear_paquete_op(READ_OP);
    // Usar agregar_a_paquete para consistencia con recibir_contenido_paquete
    agregar_a_paquete(paquete, &direccion_fisica, sizeof(int));
    agregar_a_paquete(paquete, &size, sizeof(int));
    agregar_a_paquete(paquete, &pid_ejecutando, sizeof(int));
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
    int respuesta_size = 0;
    char* contenido = recibir_buffer(&respuesta_size, fd_memoria);

    log_info(cpu_log, VERDE("PID: %d - READ - Dir Fisica: %d - Valor: %s"), pid_ejecutando, direccion_fisica, contenido);
    free(contenido);
}

void func_goto(char* valor) {
    pthread_mutex_lock(&mutex_estado_proceso);
    pc = atoi(valor);
    pthread_mutex_unlock(&mutex_estado_proceso);
}

void func_io(char* nombre_dispositivo, char* tiempo_str) {
    int tiempo = atoi(tiempo_str);  // Convertir tiempo de string a int
    
    log_info(cpu_log, ROJO("[SYSCALL]")VERDE(" Ejecutando IO - Dispositivo: '%s', Tiempo: %d"), nombre_dispositivo, tiempo);
    
    pc++; // Al PC le sumo 1 porque sino cuando vuelve de IO repite la misma instruccion IO y quedan en bucle los 4 modulos

    t_paquete* paquete = crear_paquete_op(IO_OP);
    agregar_a_paquete(paquete, nombre_dispositivo, strlen(nombre_dispositivo) + 1); // Agregar nombre del dispositivo
    agregar_entero_a_paquete(paquete, tiempo);                                      // Agregar tiempo
    agregar_entero_a_paquete(paquete, pc);                                          // Agregar pc
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    log_info(cpu_log, ROJO("[SYSCALL]")" IO enviado a Kernel - Finalizando ejecución del proceso actual");
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
    
    log_trace(cpu_log, "Ejecutando INIT_PROC - Archivo: '%s', Tamaño: %d", path, size);
    log_trace(cpu_log, "[SYSCALL] Enviando INIT_PROC_OP a Kernel...");

    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
    agregar_a_paquete(paquete, path, strlen(path)+1);   // Agrega longitud de path y path
    agregar_entero_a_paquete(paquete, size);    // Agrega el tamaño del proceso

    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);
    
    log_info(cpu_log, ROJO("[SYSCALL]")" INIT_PROC enviado a Kernel - Continuando ejecución del proceso");
}

void func_dump_memory() {
    
    pc++; //INCREMENTAR PC ANTES DE ENVIAR SYSCALL (igual que en func_io)

    // Pedirle a Kernel que ejecute el DUMP por ser una SYSCALL
    t_paquete* paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);
    agregar_entero_a_paquete(paquete, pc);  // ✅ ENVIAR PC ACTUALIZADO (igual que en func_io)
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    log_info(cpu_log, ROJO("[SYSCALL]")" DUMP_MEMORY enviado a Kernel - Finalizando ejecución del proceso");
    seguir_ejecutando = 0; // El Dump frena la ejecución porque bloquea por un tiempo o pasa a exit el proceso en caso de error
}

void func_exit() {
    int op = EXIT_OP;
    send(fd_kernel_dispatch, &op, sizeof(int), 0);
    log_info(cpu_log, ROJO("[SYSCALL]")" EXIT enviado a Kernel - Finalizando ejecución del proceso");

    seguir_ejecutando = 0; // Solo EXIT debe terminar la ejecución
}

t_instruccion* recibir_instruccion_desde_memoria() {
    log_trace(cpu_log, "[MEMORIA->CPU] Iniciando recepción de instrucción desde memoria...");

    op_code cod_op = recibir_operacion(fd_memoria);
    if (cod_op != INSTRUCCION_A_CPU_OP) {
        log_error(cpu_log, "[MEMORIA->CPU] Op_code inesperado: %d", cod_op);
        return NULL;
    }

    t_list* lista = recibir_contenido_paquete(fd_memoria);
    if (!lista || list_size(lista) < 3) {
        log_error(cpu_log, "[MEMORIA->CPU] Error al recibir paquete de instrucción");
        if (lista) list_destroy_and_destroy_elements(lista, free);
        return NULL;
    }

    log_info(cpu_log, COLOR1("[MEMORIA->CPU]")" Recibido de Memoria -> param1: '%s', param2: '%s', param3: '%s'",
        (char*)list_get(lista, 0),
        (char*)list_get(lista, 1),
        (char*)list_get(lista, 2));

    t_instruccion* instruccion_nueva = malloc(sizeof(t_instruccion));
    if (!instruccion_nueva) {
        log_error(cpu_log, "[MEMORIA->CPU] Error al asignar memoria para instrucción");
        list_destroy_and_destroy_elements(lista, free);
        return NULL;
    }

    instruccion_nueva->parametros1 = strdup((char*)list_get(lista, 0));
    instruccion_nueva->parametros2 = strdup((char*)list_get(lista, 1));
    instruccion_nueva->parametros3 = strdup((char*)list_get(lista, 2));

    log_trace(cpu_log, "[MEMORIA->CPU] ✓ INSTRUCCIÓN RECIBIDA: '%s' | Param2: '%s' | Param3: '%s'", 
        instruccion_nueva->parametros1 ? instruccion_nueva->parametros1 : "NULL",
        instruccion_nueva->parametros2 ? instruccion_nueva->parametros2 : "NULL", 
        instruccion_nueva->parametros3 ? instruccion_nueva->parametros3 : "NULL");

    list_destroy_and_destroy_elements(lista, free);
    return instruccion_nueva;
}