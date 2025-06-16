#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/funciones.h"
#include <commons/string.h>

int seguir_ejecutando = 0;     // No ejecutar hasta recibir EXEC_OP
int pid_ejecutando = -1;       // PID inválido por defecto
int pid_interrupt = -1;        // PID inválido por defecto  
int hay_interrupcion = 0;      // Sin interrupción inicialmente
int pc = 0;                    // Program Counter inicial

void ejecutar_ciclo_instruccion() {
    seguir_ejecutando = 1;
    log_trace(cpu_log, "[CICLO] ▶ Iniciando ciclo de instrucción para PID: %d", pid_ejecutando);
    
    while(seguir_ejecutando == 1){
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
        
        // Actualizar PC (excepto para GOTO)
        if(tipo_instruccion != GOTO_OP){
            pc++;
            log_trace(cpu_log, "[CICLO] PC incrementado a: %d", pc);
        } else {
            log_trace(cpu_log, "[CICLO] PC no incrementado (instrucción GOTO)");
        }
        
        // EXECUTE
        log_trace(cpu_log, "[CICLO] ▶ Ejecutando instrucción...");
        execute(tipo_instruccion, instruccion);
        
        // Liberar la memoria de la instrucción
        liberar_instruccion(instruccion);
        log_trace(cpu_log, "[CICLO] ✓ Instrucción liberada de memoria");
        
        // CHECK INTERRUPT
        if(seguir_ejecutando){
            log_trace(cpu_log, "[CICLO] Verificando interrupciones...");
            check_interrupt();
        }
        
        log_trace(cpu_log, "[CICLO] ═══ FIN CICLO ═══ (Seguir: %d)", seguir_ejecutando);
    }
    
    log_trace(cpu_log, "[CICLO] ◼ Ciclo de instrucción finalizado para PID: %d", pid_ejecutando);
}

// fetch
t_instruccion* fetch(){
    log_info(cpu_log, "## PID: %d - FETCH - Program Counter: %d", pid_ejecutando, pc);

    log_trace(cpu_log, "[FETCH] Enviando solicitud PEDIR_INSTRUCCION_OP a memoria...");
    t_paquete* paquete = crear_paquete_op(PEDIR_INSTRUCCION_OP);
    agregar_entero_a_paquete(paquete, pid_ejecutando);  // CAMBIO: PID primero
    agregar_entero_a_paquete(paquete, pc);              // CAMBIO: PC segundo
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    log_trace(cpu_log, "[FETCH] ✓ Solicitud enviada a memoria - PID: %d, PC: %d", pid_ejecutando, pc);

    // Recibir el código de operación primero
    int codigo = recibir_operacion(fd_memoria);
    log_trace(cpu_log, "[FETCH] Código de operación recibido desde memoria: %d", codigo);

    t_instruccion* instruccion = recibir_instruccion(fd_memoria);
    
    if (instruccion != NULL) {
        log_trace(cpu_log, "[FETCH] ✓ Instrucción obtenida exitosamente desde memoria");
    } else {
        log_error(cpu_log, "[FETCH] ✗ Error al recibir instrucción desde memoria");
    }

    return instruccion;
}

// decode
op_code decode(char* nombre_instruccion){
    log_trace(cpu_log, "[DECODE] Decodificando instrucción: '%s'", nombre_instruccion);
    
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
    
    log_info(cpu_log, "## PID: %d - Ejecutando: %s - %s", 
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
        default:
            log_error(cpu_log, "Instrucción desconocida\n");
        break;
    }
}


void check_interrupt(){
    if (hay_interrupcion){
        hay_interrupcion = 0;
        if(pid_ejecutando == pid_interrupt){
            seguir_ejecutando = 0;

            t_paquete* paquete_kernel = crear_paquete_op(INTERRUPCION_OP);
            agregar_entero_a_paquete(paquete_kernel, pid_ejecutando);

            enviar_paquete(paquete_kernel, fd_kernel_interrupt);
            eliminar_paquete(paquete_kernel);
        }
    }
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
