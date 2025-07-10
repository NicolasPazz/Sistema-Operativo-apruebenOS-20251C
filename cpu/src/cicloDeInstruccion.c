#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/funciones.h"
#include <commons/string.h>
#include "../headers/main.h"
int seguir_ejecutando = 0;     // No ejecutar hasta recibir EXEC_OP
int pid_ejecutando = -1;       // PID inválido por defecto
int pid_interrupt = -1;        // PID inválido por defecto  
int hay_interrupcion = 0;      // Sin interrupción inicialmente
int pc = 1;                    // Program Counter inicial (Lo dejo en 1 para que el valor coincida con la línea de los archivos de pseudocódigo)

void ejecutar_ciclo_instruccion() {
    pthread_mutex_lock(&mutex_estado_proceso);
    seguir_ejecutando = 1;
    pthread_mutex_unlock(&mutex_estado_proceso);
    log_trace(cpu_log, "[CICLO] ▶ Iniciando ciclo de instrucción para PID: %d", pid_ejecutando);

    while (seguir_ejecutando == 1) {
        log_trace(cpu_log, "[CICLO] ═══ NUEVO CICLO DE INSTRUCCIÓN ═══");
        log_trace(cpu_log, "[CICLO] PID: %d | PC: %d | Seguir: %d", pid_ejecutando, pc, seguir_ejecutando);

        // FETCH
        t_instruccion* instruccion = fetch();
        if (instruccion == NULL) {
            log_error(cpu_log, "[CICLO] ✗ Error al obtener instrucción, finalizando ciclo");
            break;
        }

        // DECODE
        op_code tipo_instruccion = decode(instruccion->parametros1);
        if (tipo_instruccion == -1) {
            log_error(cpu_log, "[CICLO] ✗ Error al decodificar instrucción, finalizando ciclo");
            liberar_instruccion(instruccion);
            break;
        }

        // EXECUTE (primero)
        log_trace(cpu_log, "[CICLO] ▶ Ejecutando instrucción...");
        execute(tipo_instruccion, instruccion);

        // Luego del execute, incrementar PC solo si no fue GOTO
        if (tipo_instruccion != GOTO_OP) {
            pc++;
            log_trace(cpu_log, "[CICLO] PC incrementado a: %d", pc);
        } else {
            log_trace(cpu_log, "[CICLO] PC no incrementado (instrucción GOTO)");
        }

        // Liberar la memoria de la instrucción
        liberar_instruccion(instruccion);
        log_trace(cpu_log, "[CICLO] ✓ Instrucción liberada de memoria");

        // CHECK INTERRUPT
        if (seguir_ejecutando) {
            log_trace(cpu_log, "[CICLO] Verificando interrupciones...");
            check_interrupt();
        }

        log_trace(cpu_log, "[CICLO] ═══ FIN CICLO ═══ (Seguir: %d)", seguir_ejecutando);
    }

    log_trace(cpu_log, "[CICLO] ◼ Ciclo de instrucción finalizado para PID: %d", pid_ejecutando);
}


// fetch
t_instruccion* fetch() {
    log_info(cpu_log, VERDE("## (PID: %d) - ")ROJO("FETCH")VERDE(" - Program Counter: %d"), pid_ejecutando, pc);

    // Enviar Paquete OP a Memoria
    log_trace(cpu_log, "[FETCH] Enviando solicitud PEDIR_INSTRUCCION_OP a memoria...");
    t_paquete* paquete = crear_paquete_op(PEDIR_INSTRUCCION_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);  //PID primero
    agregar_entero_a_paquete(paquete, pc);              //PC segundo
    
    // DEBUGGING: Enviar paquete (función void, no devuelve valor)
    enviar_paquete(paquete, fd_memoria);
    log_trace(cpu_log, "[FETCH] ✓ Paquete enviado a memoria - PID: %d, PC: %d", pid_ejecutando, pc);
    
    eliminar_paquete(paquete);

    // DEBUGGING: Agregar timeout o verificación de estado de socket antes de recibir
    log_trace(cpu_log, "[FETCH] Intentando recibir instrucción desde memoria...");
    
    // CAMBIO: Memoria ahora envía solo el buffer de datos, no un paquete con op_code
    // Por lo tanto, NO necesitamos recibir_operacion()
    t_instruccion* instruccion = recibir_instruccion_desde_memoria();
    
    if (instruccion != NULL) {
        log_trace(cpu_log, "[FETCH] ✓ Instrucción obtenida exitosamente desde memoria");
    } else {
        log_error(cpu_log, "[FETCH] ✗ Error al recibir instrucción desde memoria");
        // DEBUGGING: Agregar información adicional sobre el estado del socket
        log_error(cpu_log, "[FETCH] Estado del socket fd_memoria: %d", fd_memoria);
    }

    return instruccion;
}

// decode
op_code decode(char* nombre_instruccion) {
    log_trace(cpu_log, "[DECODE] Decodificando instrucción: '%s'", nombre_instruccion ? nombre_instruccion : "NULL");
    
    op_code resultado = -1;
    
    //INSTRUCCIONES
    if (strcmp(nombre_instruccion, "NOOP") == 0) {
        resultado = NOOP_OP;
    } else if (strcmp(nombre_instruccion, "WRITE") == 0) {
        resultado = WRITE_OP;
    } else if (strcmp(nombre_instruccion, "READ") == 0) {
        resultado = READ_OP;
    } else if (strcmp(nombre_instruccion, "GOTO") == 0) {
        resultado = GOTO_OP;
    //SYSCALLS
    } else if (strcmp(nombre_instruccion, "IO") == 0) {
        resultado = IO_OP;
    } else if (strcmp(nombre_instruccion, "INIT_PROC") == 0) {
        resultado = INIT_PROC_OP;
    } else if (strcmp(nombre_instruccion, "DUMP_MEMORY") == 0) {
        resultado = DUMP_MEMORY_OP;
    } else if (strcmp(nombre_instruccion, "EXIT") == 0) {
        resultado = EXIT_OP;
    } else if (strcmp(nombre_instruccion, "SHUTDOWN") == 0) {
        resultado = SHUTDOWN_OP;
    }
    
    if (resultado != -1) {
        log_trace(cpu_log, "[DECODE] ✓ Instrucción '%s' decodificada como op_code: %d", nombre_instruccion, resultado);
    } else {
        log_error(cpu_log, "[DECODE] ✗ Instrucción '%s' no reconocida", nombre_instruccion);
    }
    
    return resultado;
}

//execute
void execute(op_code tipo_instruccion, t_instruccion* instruccion) {
    // CAMBIO: Log obligatorio 
    char* params_str = string_new();
    if (instruccion->parametros2 && strlen(instruccion->parametros2) > 0) {
        string_append_with_format(&params_str, "%s", instruccion->parametros2);
        if (instruccion->parametros3 && strlen(instruccion->parametros3) > 0) {
            string_append_with_format(&params_str, " %s", instruccion->parametros3);
        }
    }
    
    log_info(cpu_log, VERDE("(## PID: %d) - Ejecutando: ")ROJO("%s - %s"), 
             pid_ejecutando, 
             instruccion->parametros1, 
             strlen(params_str) > 0 ? params_str : "");
    
    free(params_str);

    switch (tipo_instruccion) {
        case NOOP_OP:
            log_trace(cpu_log, "INSTRUCCION :%d", tipo_instruccion); 
            func_noop();
            break;
        case WRITE_OP:
            log_trace(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); 
            func_write(instruccion->parametros2, instruccion->parametros3);
            break;
        case READ_OP:
            log_trace(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3); 
            func_read(instruccion->parametros2, instruccion->parametros3);  
            break;
        case GOTO_OP:
            log_trace(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2); 
            func_goto(instruccion->parametros2);
            break;
        case IO_OP:
            log_trace(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s", tipo_instruccion, instruccion->parametros2); 
            func_io(instruccion->parametros2, instruccion->parametros3); 
            break;
        case INIT_PROC_OP:
            log_trace(cpu_log, "INSTRUCCION :%d - PARAMETRO 1: %s - PARAMETRO 2: %s", tipo_instruccion, instruccion->parametros2, instruccion->parametros3);
            log_debug(cpu_log, "parametros desde cpu para chequear PATH :%s, :%s",instruccion->parametros2, instruccion->parametros3);
            func_init_proc(instruccion); // en realidad son 2 parametros
            break;
        case DUMP_MEMORY_OP:
            log_trace(cpu_log, "INSTRUCCION :%d", tipo_instruccion);   
            func_dump_memory();
            break;
        case EXIT_OP:
            log_trace(cpu_log, "INSTRUCCION :%d", tipo_instruccion); 
            func_exit();
            break;
        case SHUTDOWN_OP:
            log_info(cpu_log, "SHUTDOWN_OP recibido - Finalizando CPU");
            // Cerrar todas las conexiones y liberar recursos
            if (fd_memoria != -1) close(fd_memoria);
            if (fd_kernel_dispatch != -1) close(fd_kernel_dispatch);
            if (fd_kernel_interrupt != -1) close(fd_kernel_interrupt);
            exit(EXIT_SUCCESS);
            break;
        default:
            log_error(cpu_log, "Instrucción desconocida");
        break;
    }
}

void check_interrupt() {
    pthread_mutex_lock(&mutex_estado_proceso);
    if (hay_interrupcion) {
        hay_interrupcion = 0;
        log_trace(cpu_log, "[INTERRUPT] Verificando interrupción para PID: %d mientras se ejecuta PID: %d", pid_interrupt, pid_ejecutando);
        if (pid_ejecutando == pid_interrupt) {
            log_debug(cpu_log, VERDE("[INTERRUPT]: ## (PID: %d) - Recibida interrupción, desalojando proceso"), pid_ejecutando);
            seguir_ejecutando = 0;
            pthread_mutex_unlock(&mutex_estado_proceso);

            // Limpiar TLB y cache antes de desalojar el proceso
            desalojar_proceso_tlb();
            desalojar_proceso_cache();

            t_paquete* paquete = crear_paquete_op(OK);
            pthread_mutex_lock(&mutex_estado_proceso);
            log_debug(cpu_log, VERDE("[INTERRUPT]: ## (PID: %d - PC %d) - Respuesta de OK enviada a Kernel por interrupción válida"), pid_ejecutando, pc);
            agregar_entero_a_paquete(paquete, pid_ejecutando);
            agregar_entero_a_paquete(paquete, pc);
            pthread_mutex_unlock(&mutex_estado_proceso);

            enviar_paquete(paquete, fd_kernel_interrupt);
            eliminar_paquete(paquete);

            return;
        }
        log_debug(cpu_log, VERDE("[INTERRUPT]: ## (PID: %d) - Interrupción recibida pero no corresponde al PID ejecutando (%d)"), pid_interrupt, pid_ejecutando);
        t_respuesta respuesta_error = ERROR;
        send(fd_kernel_interrupt, &respuesta_error, sizeof(t_respuesta), 0);
        log_debug(cpu_log, VERDE("[INTERRUPT]: ## (PID: %d) - Respuesta de ERROR enviada a Kernel por interrupción no válida"), pid_interrupt);
    }
    pthread_mutex_unlock(&mutex_estado_proceso);
}

// liberamos memoria
void liberar_instruccion(t_instruccion* instruccion) {
    if (instruccion != NULL) {
        if (instruccion->parametros1 != NULL) free(instruccion->parametros1);
        if (instruccion->parametros2 != NULL) free(instruccion->parametros2);
        if (instruccion->parametros3 != NULL) free(instruccion->parametros3);
        free(instruccion);
    }
}
