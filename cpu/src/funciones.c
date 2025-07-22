
#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/cache.h"
#include "../../utils/headers/sockets.h"
#include "../headers/main.h"

void func_noop() {
    log_trace(cpu_log, "## PID: %d - Acción: NOOP", pid_ejecutando);
}

// Primero se consuta a la cache y despues tlb
int calcular_tamanio_datos_seguro(char* datos) {
    if (!datos) return 0;
    
    // Buscar terminador nulo (128 chars máximo)
    int tamanio = 0;
    for (int i = 0; i < 128; i++) {
        if (datos[i] == '\0') {
            tamanio = i;
            break;
        }
        if (i == 127) {
            // Si no encontramos \0 en 127 chars, usar el string completo pero añadir \0
            tamanio = 127;
            break;
        }
    }
    return tamanio;
}

void func_write(char* direccion_logica_str, char* datos) {
    int direccion_logica = atoi(direccion_logica_str);
    int tam_pagina = cfg_memoria->TAM_PAGINA;
    int nro_pagina = direccion_logica / tam_pagina;

    // Validar y calcular tamaño de forma segura
    if (!datos) {
        log_trace(cpu_log, "PID: %d - Datos nulos en func_write", pid_ejecutando);
        return;
    }
    
    // Calcular tamaño sin usar strlen()
    int tamanio_real = calcular_tamanio_datos_seguro(datos);
    if (tamanio_real == 0) {
        log_trace(cpu_log, "PID: %d - Datos vacíos en func_write", pid_ejecutando);
        return;
    }

    if (cache_habilitada()) {
        int pos = buscar_pagina_en_cache(pid_ejecutando, nro_pagina);
        if (pos != -1) {
            //int offset = direccion_logica % tam_pagina; // WARNING: VARIABLE NO UTILIZADA
            
            log_info(cpu_log, "PID: %d - Cache HIT - Página: %d", pid_ejecutando, nro_pagina);
            
            // Pasar tamaño calculado en lugar de usar strlen() interno
            cache_modificar(pid_ejecutando, nro_pagina, direccion_logica, datos, tamanio_real);
            
            log_trace(cpu_log, "PID: %d - Cache HIT - Pagina: %d - Valor escrito: %s", pid_ejecutando, nro_pagina, datos);
            return;
        }
        log_info(cpu_log, ROJO("PID: %d - Cache Miss - Página: %d"), pid_ejecutando, nro_pagina);
    }

    int direccion_fisica = traducir_direccion_fisica(direccion_logica);

    // Usar tamaño ya calculado en lugar de strlen()
    t_paquete* paquete = crear_paquete_op(WRITE_OP);
    agregar_a_paquete(paquete, &pid_ejecutando, sizeof(int));
    agregar_a_paquete(paquete, &direccion_fisica, sizeof(int));
    agregar_a_paquete(paquete, &tamanio_real, sizeof(int));
    agregar_a_paquete(paquete, datos, tamanio_real);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    t_respuesta respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), MSG_WAITALL) != sizeof(t_respuesta)) {
        log_trace(cpu_log, "PID: %d - Error al recibir respuesta de WRITE desde Memoria", pid_ejecutando);
        exit(EXIT_FAILURE);
    }

    if (respuesta != OK) {
        log_trace(cpu_log, "PID: %d - Error en escritura de memoria: %d", pid_ejecutando, respuesta);
        exit(EXIT_FAILURE);
    }

    log_info(cpu_log, VERDE("PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %s"), pid_ejecutando, direccion_fisica, datos);

    if (cache_habilitada()) {
        t_paquete* paquete_pagina = crear_paquete_op(LEER_PAGINA_COMPLETA_OP);
        agregar_entero_a_paquete(paquete_pagina, pid_ejecutando);
        int direccion_base_pagina = direccion_fisica & ~(cfg_memoria->TAM_PAGINA - 1);
        agregar_entero_a_paquete(paquete_pagina, direccion_base_pagina);
        enviar_paquete(paquete_pagina, fd_memoria);
        eliminar_paquete(paquete_pagina);

        op_code codigo_operacion;
        if (recv(fd_memoria, &codigo_operacion, sizeof(op_code), MSG_WAITALL) != sizeof(op_code)) {
            log_trace(cpu_log, "PID: %d - Error al recibir op_code de respuesta desde Memoria", pid_ejecutando);
            exit(EXIT_FAILURE);
        }

        if (codigo_operacion != PAQUETE_OP) {
            log_trace(cpu_log, "PID: %d - Op_code inesperado en respuesta: %d", pid_ejecutando, codigo_operacion);
            exit(EXIT_FAILURE);
        }

        t_list* lista_respuesta = recibir_contenido_paquete(fd_memoria);
        if (lista_respuesta == NULL || list_size(lista_respuesta) < 1) {
            log_trace(cpu_log, "PID: %d - Error al recibir respuesta de página desde Memoria", pid_ejecutando);
            exit(EXIT_FAILURE);
        }

        char* contenido = (char*)list_get(lista_respuesta, 0);
        cache_escribir(pid_ejecutando, nro_pagina, contenido, true);   // TRUE: página contiene escritura
        list_destroy_and_destroy_elements(lista_respuesta, free);
    }
}

void func_read(char* direccion_logica_str, char* tam_str) {
    int direccion_logica = atoi(direccion_logica_str);
    int tam = atoi(tam_str);
    int tam_pagina = cfg_memoria->TAM_PAGINA;
    int nro_pagina = direccion_logica / tam_pagina;

    if (cache_habilitada()) {
        int pos = buscar_pagina_en_cache(pid_ejecutando, nro_pagina);
        if (pos != -1) {
            char* contenido = cache_leer(pid_ejecutando, nro_pagina);
            char buffer[128] = {0};
            memcpy(buffer, contenido, tam);
            buffer[tam] = '\0';
            log_info(cpu_log, "PID: %d - Cache HIT - Pagina: %d", pid_ejecutando, nro_pagina);
            log_trace(cpu_log, "PID: %d - Contenido leído (cache): %s", pid_ejecutando, buffer);
            free(contenido);
            return;
        }
        log_info(cpu_log, ROJO("PID: %d - Cache Miss - Página: %d"), pid_ejecutando, nro_pagina);
    }

    int direccion_fisica = traducir_direccion_fisica(direccion_logica);

    t_paquete* paquete = crear_paquete_op(READ_OP);
    agregar_a_paquete(paquete, &direccion_fisica, sizeof(int));
    agregar_a_paquete(paquete, &tam, sizeof(int));
    agregar_a_paquete(paquete, &pid_ejecutando, sizeof(int));
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    op_code codigo_operacion;
    if (recv(fd_memoria, &codigo_operacion, sizeof(op_code), MSG_WAITALL) != sizeof(op_code)) {
        log_trace(cpu_log, "PID: %d - Error al recibir op_code de respuesta desde Memoria", pid_ejecutando);
        exit(EXIT_FAILURE);
    }

    if (codigo_operacion != PAQUETE_OP) {
        log_trace(cpu_log, "PID: %d - Op_code inesperado en respuesta: %d", pid_ejecutando, codigo_operacion);
        exit(EXIT_FAILURE);
    }

    t_list* lista_respuesta = recibir_contenido_paquete(fd_memoria);
    if (lista_respuesta == NULL || list_size(lista_respuesta) < 1) {
        log_trace(cpu_log, "PID: %d - Error al recibir respuesta de READ desde Memoria", pid_ejecutando);
        exit(EXIT_FAILURE);
    }

    char* contenido = (char*)list_get(lista_respuesta, 0);
    char buffer[128] = {0};
    memcpy(buffer, contenido, tam);
    buffer[tam] = '\0';

    log_info(cpu_log, VERDE("PID: %d - Acción: LEER - Dirección Fisica: %d - Valor: %s"), pid_ejecutando, direccion_fisica, buffer);
    log_trace(cpu_log, "PID: %d - Contenido leído: %s", pid_ejecutando, buffer);
    
    list_destroy_and_destroy_elements(lista_respuesta, free);

    if (cache_habilitada()) {
        t_paquete* paquete_pagina = crear_paquete_op(LEER_PAGINA_COMPLETA_OP);
        agregar_entero_a_paquete(paquete_pagina, pid_ejecutando);
        int direccion_base_pagina = direccion_fisica & ~(cfg_memoria->TAM_PAGINA - 1);
        agregar_entero_a_paquete(paquete_pagina, direccion_base_pagina);
        enviar_paquete(paquete_pagina, fd_memoria);
        eliminar_paquete(paquete_pagina);

        op_code codigo_operacion_pagina;
        if (recv(fd_memoria, &codigo_operacion_pagina, sizeof(op_code), MSG_WAITALL) != sizeof(op_code)) {
            log_trace(cpu_log, "PID: %d - Error al recibir op_code de respuesta de página desde Memoria", pid_ejecutando);
            exit(EXIT_FAILURE);
        }

        if (codigo_operacion_pagina != PAQUETE_OP) {
            log_trace(cpu_log, "PID: %d - Op_code inesperado en respuesta de página: %d", pid_ejecutando, codigo_operacion_pagina);
            exit(EXIT_FAILURE);
        }

        t_list* lista_respuesta_pagina = recibir_contenido_paquete(fd_memoria);
        if (lista_respuesta_pagina == NULL || list_size(lista_respuesta_pagina) < 1) {
            log_trace(cpu_log, "PID: %d - Error al recibir respuesta de página desde Memoria", pid_ejecutando);
            exit(EXIT_FAILURE);
        }

        char* contenido_pagina = (char*)list_get(lista_respuesta_pagina, 0);
        cache_escribir(pid_ejecutando, nro_pagina, contenido_pagina, false);  // FALSE: página sincronizada con memoria
        list_destroy_and_destroy_elements(lista_respuesta_pagina, free);
    }
}

void func_goto(char* valor) {
    pthread_mutex_lock(&mutex_estado_proceso);
    pc = atoi(valor);
    pthread_mutex_unlock(&mutex_estado_proceso);
}

void func_io(char* nombre_dispositivo, char* tiempo_str) {
    int tiempo = atoi(tiempo_str);  // Convertir tiempo de string a int
    
    log_trace(cpu_log, ROJO("[SYSCALL]")VERDE(" Ejecutando IO - Dispositivo: '%s', Tiempo: %d"), nombre_dispositivo, tiempo);
    
    pc++; // Al PC le sumo 1 porque sino cuando vuelve de IO repite la misma instruccion IO y quedan en bucle los 4 modulos

    t_paquete* paquete = crear_paquete_op(IO_OP);
    agregar_a_paquete(paquete, nombre_dispositivo, strlen(nombre_dispositivo) + 1); // Agregar nombre del dispositivo
    agregar_entero_a_paquete(paquete, tiempo);                                      // Agregar tiempo
    agregar_entero_a_paquete(paquete, pc);                                          // Agregar pc
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    log_trace(cpu_log, ROJO("[SYSCALL]")" IO enviado a Kernel - Finalizando ejecución del proceso actual");
    seguir_ejecutando = 0;
}

void func_init_proc(t_instruccion* instruccion) {
    char* path = instruccion->parametros2;
    char* size_str = instruccion->parametros3;
    int size = atoi(size_str);

    if (!path || !size_str) {
        log_trace(cpu_log, "[SYSCALL] INIT_PROC recibido con parámetros inválidos.");
        return;
    }
    
    log_trace(cpu_log, "Ejecutando INIT_PROC - Archivo: '%s', Tamaño: %d", path, size);
    log_trace(cpu_log, "[SYSCALL] Enviando INIT_PROC_OP a Kernel...");

    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
    agregar_a_paquete(paquete, path, strlen(path)+1);   // Agrega longitud de path y path
    agregar_entero_a_paquete(paquete, size);    // Agrega el tamaño del proceso

    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);
    
    log_trace(cpu_log, ROJO("[SYSCALL]")" INIT_PROC enviado a Kernel - Continuando ejecución del proceso");
}

void func_dump_memory() {
    
    pc++; //INCREMENTAR PC ANTES DE ENVIAR SYSCALL (igual que en func_io)

    // Pedirle a Kernel que ejecute el DUMP por ser una SYSCALL
    t_paquete* paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);
    agregar_entero_a_paquete(paquete, pc);  //   ENVIAR PC ACTUALIZADO (igual que en func_io)
    enviar_paquete(paquete, fd_kernel_dispatch);
    eliminar_paquete(paquete);

    log_trace(cpu_log, ROJO("[SYSCALL]")" DUMP_MEMORY enviado a Kernel - Finalizando ejecución del proceso");
    seguir_ejecutando = 0; // El Dump frena la ejecución porque bloquea por un tiempo o pasa a exit el proceso en caso de error
}

void func_exit() {
    // Limpiar TLB y cache antes de terminar el proceso
    desalojar_proceso_tlb(pid_ejecutando);
    desalojar_proceso_cache(pid_ejecutando);
    
    int op = EXIT_OP;
    send(fd_kernel_dispatch, &op, sizeof(int), 0);
    log_trace(cpu_log, ROJO("[SYSCALL]")" EXIT enviado a Kernel - Finalizando ejecución del proceso");

    seguir_ejecutando = 0; // Solo EXIT debe terminar la ejecución
}

t_instruccion* recibir_instruccion_desde_memoria() {
    log_trace(cpu_log, "[MEMORIA->CPU] Iniciando recepción de instrucción desde memoria...");

    op_code cod_op = recibir_operacion(fd_memoria);
    if (cod_op != INSTRUCCION_A_CPU_OP) {
        log_trace(cpu_log, "[MEMORIA->CPU] Op_code inesperado: %d", cod_op);
        return NULL;
    }

    t_list* lista = recibir_contenido_paquete(fd_memoria);
    if (!lista || list_size(lista) < 3) {
        log_trace(cpu_log, "[MEMORIA->CPU] Error al recibir paquete de instrucción");
        if (lista) list_destroy_and_destroy_elements(lista, free);
        return NULL;
    }

    log_trace(cpu_log, COLOR1("[MEMORIA->CPU]")" Recibido de Memoria -> param1: '%s', param2: '%s', param3: '%s'",
        (char*)list_get(lista, 0),
        (char*)list_get(lista, 1),
        (char*)list_get(lista, 2));

    t_instruccion* instruccion_nueva = malloc(sizeof(t_instruccion));
    if (!instruccion_nueva) {
        log_trace(cpu_log, "[MEMORIA->CPU] Error al asignar memoria para instrucción");
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
