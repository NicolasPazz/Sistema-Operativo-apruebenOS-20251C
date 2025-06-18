#include "../headers/comunicacion.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/manejo_memoria.h"
#include "../headers/manejo_swap.h"
#include "../headers/metricas.h"
#include <commons/log.h>
#include <commons/string.h>
#include <string.h>
#include <unistd.h>

/////////////////////////////// Inicializacion de variables globales ///////////////////////////////
int fd_memoria;
int fd_kernel;
int fd_cpu;
extern t_log* logger;
extern t_config_memoria* cfg;
extern t_sistema_memoria* sistema_memoria;

typedef struct {
    int fd;
    char* server_name;
} t_procesar_conexion_args;

// Declaración de funciones internas
void* leer_pagina_completa(int pid, int direccion_fisica);

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
                *((int*)(sistema_memoria->memoria_principal + direccion)) = valor;
            
            // Enviar respuesta de éxito
                t_respuesta_memoria respuesta = OK;
                send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
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
                int valor = *((int*)(sistema_memoria->memoria_principal + direccion));
            
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

            // ========== RECEPCIÓN DE PARÁMETROS ==========
            t_list* lista = recibir_contenido_paquete(cliente_socket);
            
            int pid = *(int*)list_get(lista, 0);
            char* nombre_proceso = strdup((char*)list_get(lista, 1));
            int tamanio = *(int*)list_get(lista, 2);
            
            log_debug(logger, "Inicialización de proceso solicitada - PID: %d, Tamaño: %d, Nombre: '%s'", 
                      pid, tamanio, nombre_proceso);

            // ========== EJECUCIÓN DEL PROCESO DE CREACIÓN ==========
            t_resultado_memoria resultado = crear_proceso_en_memoria(pid, tamanio, nombre_proceso);
            
            // ========== CARGA DE INSTRUCCIONES ==========
            if (resultado == MEMORIA_OK) {
                // Construir path completo para instrucciones
                char* path_completo = string_from_format("%s%s", cfg->PATH_INSTRUCCIONES, nombre_proceso);
                
                // Cargar instrucciones desde archivo
                t_process_instructions* instrucciones = load_process_instructions(pid, path_completo);
                if (instrucciones != NULL) {
                    char* pid_key = string_itoa(pid);
                    dictionary_put(sistema_memoria->process_instructions, pid_key, instrucciones);
                    log_debug(logger, "PID: %d - Instrucciones cargadas desde %s", pid, path_completo);
                    free(pid_key);
                } else {
                    log_warning(logger, "PID: %d - No se pudieron cargar instrucciones desde %s", pid, path_completo);
                }
                 
                free(path_completo);
            }

            // ========== LIBERACIÓN DE MEMORIA TEMPORAL ==========
            free(nombre_proceso);
            list_destroy_and_destroy_elements(lista, free);

            // ========== ENVÍO DE RESPUESTA ==========
            t_respuesta_memoria respuesta = (resultado == MEMORIA_OK) ? OK : ERROR;
            
            if (resultado == MEMORIA_OK) {
                log_info(logger, "Enviando respuesta OK a cliente (fd=%d) - Proceso %d creado exitosamente", 
                         cliente_socket, pid);
            } else {
                log_info(logger, "Enviando respuesta ERROR a cliente (fd=%d) - Falló creación del proceso %d", 
                         cliente_socket, pid);
            }
            
            send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
            break;
        }

        case DUMP_MEMORY_OP: {
            log_debug(logger, "DUMP_MEMORY_OP recibido");

            // Recibir PID
            int pid;
            recv_data(cliente_socket, &pid, sizeof(int));
            
            // Procesar memory dump
            t_resultado_memoria resultado = procesar_memory_dump(pid);
            
            // Enviar respuesta
            t_respuesta_memoria respuesta = (resultado == MEMORIA_OK) ? OK : ERROR;
            log_info(logger, "Enviando respuesta %s a cliente (fd=%d)", 
                    (respuesta == OK) ? "OK" : "ERROR", cliente_socket);
            send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
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
                t_respuesta_memoria respuesta = OK;
                log_info(logger, "Enviando respuesta OK a cliente (fd=%d)", cliente_socket);
                send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
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

            // Recibir PID y PC desde paquete (para coincidir con CPU)
            t_list* lista = recibir_2_enteros_sin_op(cliente_socket);
            int pid = (int)(intptr_t)list_get(lista, 0);  // PID primero
            int pc = (int)(intptr_t)list_get(lista, 1);   // PC segundo
            list_destroy(lista);
            
            log_debug(logger, "Instrucción solicitada - PID: %d, PC: %d", pid, pc);
        
            // Obtener y enviar la instrucción
            enviar_instruccion_a_cpu(pid, pc, cliente_socket);
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

        // ============================================================================
        // HANDLERS PARA LOS 4 TIPOS DE ACCESO ESPECÍFICOS DE LA CONSIGNA
        // ============================================================================

        case ACCESO_TABLA_PAGINAS_OP: {
            log_debug(logger, "ACCESO_TABLA_PAGINAS_OP recibido");

            // Recibir parámetros: PID y número de página
            t_list* lista = recibir_2_enteros_sin_op(cliente_socket);
            int pid = (int)(intptr_t)list_get(lista, 0);
            int numero_pagina = (int)(intptr_t)list_get(lista, 1);
            list_destroy(lista);
            
            log_debug(logger, "Acceso a tabla de páginas - PID: %d, Página: %d", pid, numero_pagina);
            
            // Realizar acceso a tabla de páginas
            int numero_marco = acceso_tabla_paginas(pid, numero_pagina);
            
            // Enviar respuesta
            if (numero_marco != -1) {
                send_data(cliente_socket, &numero_marco, sizeof(int));
                log_debug(logger, "Marco %d enviado para PID: %d, Página: %d", numero_marco, pid, numero_pagina);
            } else {
                int error = -1;
                send_data(cliente_socket, &error, sizeof(int));
                log_error(logger, "Error en acceso a tabla de páginas - PID: %d, Página: %d", pid, numero_pagina);
            }
            break;
        }

        case ACCESO_ESPACIO_USUARIO_OP: {
            log_debug(logger, "ACCESO_ESPACIO_USUARIO_OP recibido");

            // Recibir parámetros del paquete
            t_list* lista = recibir_contenido_paquete(cliente_socket);
            
            char* pid_str = (char*)list_get(lista, 0);
            char* direccion_str = (char*)list_get(lista, 1);
            char* tamanio_str = (char*)list_get(lista, 2);
            char* es_escritura_str = (char*)list_get(lista, 3);
            
            int pid = atoi(pid_str);
            int direccion_fisica = atoi(direccion_str);
            int tamanio = atoi(tamanio_str);
            bool es_escritura = (strcmp(es_escritura_str, "true") == 0);
            
            log_debug(logger, "Acceso a espacio de usuario - PID: %d, Dir: %d, Tamaño: %d, Escritura: %s", 
                     pid, direccion_fisica, tamanio, es_escritura ? "true" : "false");
            
            if (es_escritura) {
                // Para escritura, recibir datos adicionales
                void* datos = list_get(lista, 4);
                bool resultado = acceso_espacio_usuario_escritura(pid, direccion_fisica, tamanio, datos);
                
                // Enviar respuesta
                t_respuesta_memoria respuesta = resultado ? OK : ERROR;
                send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
                
                log_debug(logger, "Escritura en espacio de usuario %s - PID: %d", 
                         resultado ? "exitosa" : "fallida", pid);
            } else {
                // Para lectura, devolver datos
                void* datos = acceso_espacio_usuario_lectura(pid, direccion_fisica, tamanio);
                
                if (datos != NULL) {
                    send_data(cliente_socket, datos, tamanio);
                    free(datos);
                    log_debug(logger, "Lectura de espacio de usuario exitosa - PID: %d", pid);
                } else {
                    t_respuesta_memoria respuesta = ERROR;
                    send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
                    log_error(logger, "Error en lectura de espacio de usuario - PID: %d", pid);
                }
            }
            
            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case LEER_PAGINA_COMPLETA_OP: {
            log_debug(logger, "LEER_PAGINA_COMPLETA_OP recibido");

            // Recibir parámetros: PID y dirección física
            t_list* lista = recibir_2_enteros_sin_op(cliente_socket);
            int pid = (int)(intptr_t)list_get(lista, 0);
            int direccion_fisica = (int)(intptr_t)list_get(lista, 1);
            list_destroy(lista);
            
            log_debug(logger, "Leer página completa - PID: %d, Dir. Física: %d", pid, direccion_fisica);
            
            // Leer página completa
            void* pagina_completa = leer_pagina_completa(pid, direccion_fisica);
            
            if (pagina_completa != NULL) {
                // Enviar página completa
                send_data(cliente_socket, pagina_completa, cfg->TAM_PAGINA);
                free(pagina_completa);
                log_debug(logger, "Página completa enviada - PID: %d, Dir: %d", pid, direccion_fisica);
            } else {
                // Enviar error
                t_respuesta_memoria respuesta = ERROR;
                send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
                log_error(logger, "Error al leer página completa - PID: %d, Dir: %d", pid, direccion_fisica);
            }
            break;
        }

        case ACTUALIZAR_PAGINA_COMPLETA_OP: {
            log_debug(logger, "ACTUALIZAR_PAGINA_COMPLETA_OP recibido");

            // Recibir parámetros del paquete
            t_list* lista = recibir_contenido_paquete(cliente_socket);
            
            char* pid_str = (char*)list_get(lista, 0);
            char* direccion_str = (char*)list_get(lista, 1);
            
            int pid = atoi(pid_str);
            int direccion_fisica = atoi(direccion_str);
            
            // El contenido de la página viene como el tercer elemento
            void* contenido_pagina = list_get(lista, 2);
            
            log_debug(logger, "Actualizar página completa - PID: %d, Dir. Física: %d", pid, direccion_fisica);
            
            // Actualizar página completa
            bool resultado = actualizar_pagina_completa(pid, direccion_fisica, contenido_pagina);
            
            // Enviar respuesta
            t_respuesta_memoria respuesta = resultado ? OK : ERROR;
            send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
            
            log_debug(logger, "Actualización de página completa %s - PID: %d", 
                     resultado ? "exitosa" : "fallida", pid);
            
            list_destroy_and_destroy_elements(lista, free);
            break;
        }

        case CHECK_MEMORY_SPACE_OP: {
            log_debug(logger, "CHECK_MEMORY_SPACE_OP recibido");

            // Recibir tamaño solicitado
            int tamanio;
            recv_data(cliente_socket, &tamanio, sizeof(int));
            
            // Verificar espacio disponible
            bool hay_espacio = verificar_espacio_disponible(tamanio);
            
            // Enviar respuesta
            t_respuesta_memoria respuesta = hay_espacio ? OK : ERROR;
            send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
            
            log_debug(logger, "Respuesta de verificación de espacio: %s", 
                     hay_espacio ? "OK" : "ERROR");
            break;
        }

        default:
            log_error(logger, "Codigo de operacion desconocido: %d", cop);
            break;
    }
}