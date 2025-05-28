#include "../headers/comunicacion.h"
#include <commons/string.h>

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


int iniciar_conexiones_memoria(char* PUERTO_ESCUCHA, t_log* logger_param) {
    if (logger_param == NULL) {
        printf("Error: logger no inicializado\n");
        exit(EXIT_FAILURE);
    }

    fd_memoria = iniciar_servidor(PUERTO_ESCUCHA, logger_param, "Server Memoria iniciado");

    if (fd_memoria == -1) {
        log_error(logger_param, "No se pudo iniciar el servidor de Memoria");
        exit(EXIT_FAILURE);
    }

    log_debug(logger_param, "Esperando conexiones entrantes en Memoria...");
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
            log_debug(logger, "## CPU Conectado - FD del socket: %d", cliente_socket);
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

    log_warning(logger, "El cliente (fd=%d) se desconectó de %s", cliente_socket, server_name);

    if (cliente_socket == fd_kernel) {
        log_warning(logger, "Se desconectó el Kernel. Finalizando Memoria...");
        cerrar_programa();
        exit(EXIT_SUCCESS);
    }

    close(cliente_socket);
}

void procesar_cod_ops(op_code cop, int cliente_socket) {
    switch (cop) {
        case MENSAJE_OP:
            log_debug(logger, "MENSAJE_OP recibido");

            // Recibir un Mensaje char* y loguearlo
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
                int pid, direccion, valor;
                recv_data(cliente_socket, &pid, sizeof(int));
                recv_data(cliente_socket, &direccion, sizeof(int));
                recv_data(cliente_socket, &valor, sizeof(int));
            
            // Para el Check 2, simulamos la escritura
                log_info(logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %ld", pid, direccion, sizeof(int));
            
            // Actualizar métrica
                actualizar_metricas(pid, "MEMORY_WRITE");
            
            // Simular escritura en memoria
                *((int*)leer_pagina(direccion)) = valor;
            
            // Enviar respuesta de éxito
                char* respuesta = "OK";
                send_string(cliente_socket, respuesta);
            break;
        }

        case READ_OP: {
            log_debug(logger, "READ_OP recibido");

            // Recibir parámetros (PID y dirección)
                int pid, direccion;
                recv_data(cliente_socket, &pid, sizeof(int));
                recv_data(cliente_socket, &direccion, sizeof(int));
            
            // Para el checkpoint 2, simulamos la lectura
                log_info(logger, "## PID: %d - Lectura - Dir. Física: %d - Tamaño: %ld", pid, direccion, sizeof(int));
            
            // Actualizar métrica
                actualizar_metricas(pid, "MEMORY_READ");
            
            // Simular lectura de memoria
                int valor = *((int*)leer_pagina(direccion));
            
            // Enviar el valor leído
                send_data(cliente_socket, &valor, sizeof(int));
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

            // Recibir parámetros (PID, tamaño, nombre del proceso)
                int pid, tamanio;
                char* nombre_proceso;
                
                recv_data(cliente_socket, &pid, sizeof(pid));
                recv_data(cliente_socket, &tamanio, sizeof(tamanio));
                recv_string(cliente_socket, &nombre_proceso);
                
                log_debug(logger, "Inicialización de proceso solicitada - PID: %d, Tamaño: %d, Nombre: %s", pid, tamanio, nombre_proceso);
            
            // Construir el path relativo concatenando "scripts/" con el nombre del proceso
                char* path_completo = string_from_format("scripts/%s", nombre_proceso);
                log_debug(logger, "Path construido: %s", path_completo);
            
            // Verificar espacio disponible en memoria
                int memoria_disponible = get_available_memory();
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
                    load_process_instructions(pid, path_completo);
                }
                
                // Liberar memoria de los strings
                free(nombre_proceso);
                free(path_completo);
            
            // Enviar respuesta
                t_respuesta_memoria respuesta_enum = (resultado == 0) ? OK : ERROR;
                send(cliente_socket, &respuesta_enum, sizeof(t_respuesta_memoria), 0);
            break;
        }

        case DUMP_MEMORY_OP: {
            log_debug(logger, "DUMP_MEMORY_OP recibido");

            // Recibir PID
                int pid;
                recv_data(cliente_socket, &pid, sizeof(int));
            
            // Log obligatorio
                log_info(logger, "## PID: %d - Memory Dump solicitado", pid);
            
            // Para el checkpoint 2, enviamos una respuesta OK
                char* respuesta = "OK";
                send_string(cliente_socket, respuesta);
            break;
        }

        case EXIT_OP: {
            log_debug(logger, "EXIT_OP recibido");

            // Recibir PID
                int pid;
                recv_data(cliente_socket, &pid, sizeof(int));
            
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
            int pid, pc;
            recv_data(cliente_socket, &pid, sizeof(int));
            recv_data(cliente_socket, &pc, sizeof(int));
            
            log_debug(logger, "Instrucción solicitada - PID: %d, PC: %d", pid, pc);
        
            // Obtener la instrucción
            t_instruccion* instruccion = get_instruction(pid, pc);
            
            if (instruccion != NULL) {
                // Log obligatorio con formato correcto
                char* args_log = string_new();
                if (instruccion->parametros2 && strlen(instruccion->parametros2) > 0) {
                    string_append_with_format(&args_log, " %s", instruccion->parametros2);
                    if (instruccion->parametros3 && strlen(instruccion->parametros3) > 0) {
                        string_append_with_format(&args_log, " %s", instruccion->parametros3);
                    }
                }
                log_info(logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s%s", 
                         pid, pc, instruccion->parametros1, args_log);
                free(args_log);

                // usamos la instruccion
                t_paquete* paquete = crear_paquete_op(PEDIR_INSTRUCCION_OP);

                // siempre 3 params
                char* p1 = instruccion->parametros1 ? instruccion->parametros1 : "";
                char* p2 = instruccion->parametros2 ? instruccion->parametros2 : "";
                char* p3 = instruccion->parametros3 ? instruccion->parametros3 : "";
                
                // orden fijo
                agregar_a_paquete(paquete, p1, strlen(p1) + 1);
                agregar_a_paquete(paquete, p2, strlen(p2) + 1);
                agregar_a_paquete(paquete, p3, strlen(p3) + 1);

                // estandarizamos protocolo
                enviar_paquete(paquete, cliente_socket);
                eliminar_paquete(paquete);
            }
            break;
        }

        case PEDIR_CONFIG_CPU_OP: {
            log_debug(logger, "PEDIR_CONFIG_CPU_OP recibido");

            // Enviar la configuración necesaria para la CPU
                int entradas_por_tabla = cfg->ENTRADAS_POR_TABLA;
                int tam_pagina = cfg->TAM_PAGINA;
                int cantidad_niveles = cfg->CANTIDAD_NIVELES;
            
            // Enviamos los valores
                send_data(cliente_socket, &entradas_por_tabla, sizeof(int));
                send_data(cliente_socket, &tam_pagina, sizeof(int));
                send_data(cliente_socket, &cantidad_niveles, sizeof(int));
                
                log_debug(logger, "Configuración enviada a CPU: Entradas por tabla: %d, Tamaño página: %d, Niveles: %d",
                        entradas_por_tabla, tam_pagina, cantidad_niveles);
            break;
        }

        default:
            log_error(logger, "Codigo de operacion desconocido: %d", cop);
            break;
    }
}