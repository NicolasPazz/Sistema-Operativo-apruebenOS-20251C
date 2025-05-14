#include "../headers/comunicacion.h"

/////////////////////////////// Inicializacion de variables globales ///////////////////////////////


int fd_memoria;
int fd_kernel;
int fd_cpu;
extern t_log* logger;
extern t_config_memoria* cfg;

typedef struct {
    int fd;
    char* server_name;
} t_procesar_conexion_args;


int iniciar_conexiones_memoria(char* PUERTO_ESCUCHA, t_log* logger) {
    if (logger == NULL) {
        printf("Error: Logger no inicializado\n");
        exit(EXIT_FAILURE);
    }

    fd_memoria = iniciar_servidor(PUERTO_ESCUCHA, logger, "Server Memoria iniciado");

    if (fd_memoria == -1) {
        log_error(logger, "No se pudo iniciar el servidor de Memoria");
        exit(EXIT_FAILURE);
    }

    log_debug(logger, "Esperando conexiones entrantes en Memoria...");
    return fd_memoria; // Devuelve el socket del servidor
}

int server_escuchar(char* server_name, int server_socket) {
    if (logger == NULL) {
        printf("Error: Logger no inicializado en server_escuchar\n");
        return 0;
    }
    int cliente_socket = esperar_cliente(server_socket, logger);

    if (cliente_socket != -1) {
        pthread_t hilo;
        t_procesar_conexion_args* args = malloc(sizeof(t_procesar_conexion_args));
        args->fd = cliente_socket;
        args->server_name = server_name;
        pthread_create(&hilo, NULL, (void*) procesar_conexion, (void*) args);
        pthread_detach(hilo);
        // que se quede esperando los cop -> 
        return 1;
    }
    return 0;
}

void procesar_conexion(void* void_args) {
    t_procesar_conexion_args* args = (t_procesar_conexion_args*) void_args;
    if (args == NULL) {
        log_error(logger, "Error: Argumentos de conexion nulos");
        return;
    }

    int cliente_socket = args->fd;
    char* server_name = args->server_name;
    free(args);

    int handshake = -1;

    if (recv(cliente_socket, &handshake, sizeof(int), 0) <= 0) {
        log_error(logger, "Error al recibir handshake del cliente (fd=%d): %s", cliente_socket, strerror(errno));
        close(cliente_socket);
        return;
    }
    switch (handshake) {
        case HANDSHAKE_MEMORIA_KERNEL:
            log_info(logger, "## Kernel Conectado - FD del socket: %d", cliente_socket);
            fd_kernel = cliente_socket;
            break;

        case HANDSHAKE_MEMORIA_CPU:
            log_debug(logger, "HANDSHAKE_MEMORIA_CPU: Se conecto una CPU (fd=%d)", cliente_socket);
            fd_cpu = cliente_socket;
            break;

        default:
            log_warning(logger, "Handshake invalido recibido (fd=%d): %d", cliente_socket, handshake);
            close(cliente_socket);
            return;
    }

    log_debug(logger, "Conexion procesada exitosamente para %s (fd=%d)", server_name, cliente_socket);
    // manejo aca los codops
    op_code cop;
    while (recv(cliente_socket, &cop, sizeof(op_code), 0) > 0) {
        procesar_cod_ops(cop, cliente_socket);
    }

    log_warning(logger, "El cliente (fd=%d) se desconecto de %s", cliente_socket, server_name);
    close(cliente_socket);
}

void procesar_cod_ops(op_code cop, int cliente_socket) {
    switch (cop) {
        case MENSAJE_OP:
            log_debug(logger, "MENSAJE_OP recibido");
            // Simple mensaje de prueba
            char* mensaje;
            recv_string(cliente_socket, &mensaje);
            log_debug(logger, "Mensaje recibido: %s", mensaje);
            free(mensaje);
            break;

        case PAQUETE_OP:
            log_debug(logger, "PAQUETE_OP recibido");
            // No implementado para el checkpoint 2
            break;

        case NOOP_OP:
            log_debug(logger, "NOOP_OP recibido");
            // No implementado para el checkpoint 2
            break;

        case WRITE_OP: {
            log_debug(logger, "WRITE_OP recibido");
            // Recibir parámetros (PID, dirección y valor)
            uint32_t pid, direccion, valor;
            recv_data(cliente_socket, &pid, sizeof(uint32_t));
            recv_data(cliente_socket, &direccion, sizeof(uint32_t));
            recv_data(cliente_socket, &valor, sizeof(uint32_t));
            
            // Para el checkpoint 2, simplemente simulamos la escritura
            log_info(logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %ld", 
                    pid, direccion, sizeof(uint32_t));
            
            // Actualizar métrica
            actualizar_metricas(pid, "MEMORY_WRITE");
            
            // Simular escritura en memoria
            *((uint32_t*)leer_pagina(direccion)) = valor;
            
            // Enviar respuesta de éxito
            char* respuesta = "OK";
            send_string(cliente_socket, respuesta);
            break;
        }

        case READ_OP: {
            log_debug(logger, "READ_OP recibido");
            // Recibir parámetros (PID y dirección)
            uint32_t pid, direccion;
            recv_data(cliente_socket, &pid, sizeof(uint32_t));
            recv_data(cliente_socket, &direccion, sizeof(uint32_t));
            
            // Para el checkpoint 2, simplemente simulamos la lectura
            log_info(logger, "## PID: %d - Lectura - Dir. Física: %d - Tamaño: %ld", 
                    pid, direccion, sizeof(uint32_t));
            
            // Actualizar métrica
            actualizar_metricas(pid, "MEMORY_READ");
            
            // Simular lectura de memoria
            uint32_t valor = *((uint32_t*)leer_pagina(direccion));
            
            // Enviar el valor leído
            send_data(cliente_socket, &valor, sizeof(uint32_t));
            break;
        }

        case GOTO_OP:
            log_debug(logger, "GOTO_OP recibido");
            // No implementado para el checkpoint 2
            break;

        case IO_OP:
            log_debug(logger, "IO_OP recibido");
            // No implementado para el checkpoint 2
            break;

        case INIT_PROC_OP: {
            log_debug(logger, "INIT_PROC_OP recibido");
            // Recibir parámetros (PID, tamaño, ruta de instrucciones)
            uint32_t pid, tamanio;
            char* instrucciones_path;
            
            recv_data(cliente_socket, &pid, sizeof(uint32_t));
            recv_data(cliente_socket, &tamanio, sizeof(uint32_t));
            recv_string(cliente_socket, &instrucciones_path);
            
            log_debug(logger, "Inicialización de proceso solicitada - PID: %d, Tamaño: %d, Archivo: %s", 
                     pid, tamanio, instrucciones_path);
            
            // Verificar espacio disponible en memoria
            uint32_t memoria_disponible = get_available_memory();
            log_debug(logger, "Memoria disponible: %d bytes", memoria_disponible);
            
            int resultado;
            if (memoria_disponible >= tamanio) {
                // Hay suficiente memoria disponible
                log_debug(logger, "Hay suficiente memoria disponible para el proceso (necesita %d bytes, hay %d bytes)", 
                         tamanio, memoria_disponible);
                // Para el checkpoint 2, siempre aceptamos la inicialización
                resultado = initialize_process(pid, tamanio);
            } else {
                // No hay suficiente memoria disponible
                log_error(logger, "No hay suficiente memoria para inicializar el proceso (necesita %d bytes, hay %d bytes)", 
                         tamanio, memoria_disponible);
                resultado = -1;
            }
            
            // Cargar las instrucciones del proceso si se pudo inicializar
            if (resultado == 0) {
                load_process_instructions(pid, instrucciones_path);
            }
            
            free(instrucciones_path);
            
            // Enviar respuesta
            if (resultado == 0) {
                char* respuesta = "OK";
                send_string(cliente_socket, respuesta);
            } else {
                char* respuesta = "ERROR";
                send_string(cliente_socket, respuesta);
            }
            break;
        }

        case DUMP_MEMORY_OP: {
            log_debug(logger, "DUMP_MEMORY_OP recibido");
            // Recibir PID
            uint32_t pid;
            recv_data(cliente_socket, &pid, sizeof(uint32_t));
            
            // Log obligatorio
            log_info(logger, "## PID: %d - Memory Dump solicitado", pid);
            
            // Para el checkpoint 2, simplemente enviamos una respuesta OK
            char* respuesta = "OK";
            send_string(cliente_socket, respuesta);
            break;
        }

        case EXIT_OP: {
            log_debug(logger, "EXIT_OP recibido");
            // Recibir PID
            uint32_t pid;
            recv_data(cliente_socket, &pid, sizeof(uint32_t));
            
            log_debug(logger, "Finalización de proceso solicitada - PID: %d", pid);
            
            // Finalizar el proceso
            finalize_process(pid);
            
            // Enviar respuesta
            char* respuesta = "OK";
            send_string(cliente_socket, respuesta);
            break;
        }

        case EXEC_OP:
            log_debug(logger, "EXEC_OP recibido");
            // No implementado para el checkpoint 2
            break;

        case INTERRUPCION_OP:
            log_debug(logger, "INTERRUPCION_OP recibido");
            // No implementado para el checkpoint 2
            break;

        case PEDIR_INSTRUCCION_OP: {
            log_debug(logger, "PEDIR_INSTRUCCION_OP recibido");
            // Recibir PID y PC
            uint32_t pid, pc;
            recv_data(cliente_socket, &pid, sizeof(uint32_t));
            recv_data(cliente_socket, &pc, sizeof(uint32_t));
            
            log_debug(logger, "Instrucción solicitada - PID: %d, PC: %d", pid, pc);
            
            // Obtener la instrucción
            t_instruccion* instruccion = NULL;
            op_code tipo_op = NOOP_OP;
            
            // Primero necesitamos obtener el tipo de la instrucción extendida
            t_process_instructions* process_inst = NULL;
            for (int i = 0; i < list_size(process_instructions_list); i++) {
                t_process_instructions* p = list_get(process_instructions_list, i);
                if (p->pid == pid) {
                    process_inst = p;
                    break;
                }
            }
            
            if (process_inst != NULL && pc < list_size(process_inst->instructions)) {
                t_extended_instruccion* extended_instr = list_get(process_inst->instructions, pc);
                tipo_op = extended_instr->tipo;
                
                log_debug(logger, "Tipo de instrucción encontrado: %d", tipo_op);
                
                // Ahora obtenemos la instrucción base para enviar al CPU
                instruccion = get_instruction(pid, pc);
            }
            
            if (instruccion != NULL) {
                log_debug(logger, "Instrucción encontrada - Tipo: %d, Params: '%s', '%s', '%s'", 
                          tipo_op, 
                          instruccion->parametros1, 
                          instruccion->parametros2, 
                          instruccion->parametros3);
                
                // Crear paquete con la instrucción
                t_paquete* paquete = crear_paquete_op(tipo_op);
                
                // Agregar los parámetros al paquete
                if (instruccion->parametros1 && strlen(instruccion->parametros1) > 0)
                    agregar_a_paquete(paquete, instruccion->parametros1, strlen(instruccion->parametros1) + 1);
                
                if (instruccion->parametros2 && strlen(instruccion->parametros2) > 0)
                    agregar_a_paquete(paquete, instruccion->parametros2, strlen(instruccion->parametros2) + 1);
                
                if (instruccion->parametros3 && strlen(instruccion->parametros3) > 0)
                    agregar_a_paquete(paquete, instruccion->parametros3, strlen(instruccion->parametros3) + 1);
                
                // Enviar la instrucción
                enviar_paquete(paquete, cliente_socket);
                eliminar_paquete(paquete);
                
                // Liberar la instrucción (es una copia creada por get_instruction)
                free(instruccion->parametros1);
                free(instruccion->parametros2);
                free(instruccion->parametros3);
                free(instruccion);
            } else {
                // Si no se encontró la instrucción, enviamos una instrucción NOOP
                log_warning(logger, "No se encontró instrucción para PID: %d, PC: %d - Enviando NOOP", pid, pc);
                t_paquete* paquete = crear_paquete_op(NOOP_OP);
                enviar_paquete(paquete, cliente_socket);
                eliminar_paquete(paquete);
            }
            break;
        }

        case PEDIR_CONFIG_CPU_OP: {
            log_debug(logger, "PEDIR_CONFIG_CPU_OP recibido");
            // Enviar la configuración necesaria para la CPU
            uint32_t entradas_por_tabla = cfg->ENTRADAS_POR_TABLA;
            uint32_t tam_pagina = cfg->TAM_PAGINA;
            uint32_t cantidad_niveles = cfg->CANTIDAD_NIVELES;
            
            // Enviamos los valores
            send_data(cliente_socket, &entradas_por_tabla, sizeof(uint32_t));
            send_data(cliente_socket, &tam_pagina, sizeof(uint32_t));
            send_data(cliente_socket, &cantidad_niveles, sizeof(uint32_t));
            
            log_debug(logger, "Configuración enviada a CPU: Entradas por tabla: %d, Tamaño página: %d, Niveles: %d",
                     entradas_por_tabla, tam_pagina, cantidad_niveles);
            break;
        }

        default:
            log_error(logger, "Codigo de operacion desconocido: %d", cop);
            break;
    }
}