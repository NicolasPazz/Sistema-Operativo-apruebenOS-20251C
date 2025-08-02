#include "../headers/comunicacion.h"
#include "../headers/estructuras.h"
#include "../headers/init_memoria.h"
#include "../headers/manejo_memoria.h"
#include "../headers/monitor_memoria.h"
#include "../headers/manejo_swap.h"
#include "../headers/metricas.h"
#include "../headers/interfaz_memoria.h"
#include <commons/log.h>
#include <commons/string.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "../../utils/headers/utils.h"
#include "../../utils/headers/sockets.h"
#include "../../utils/headers/serializacion.h"
#include "../../utils/headers/types.h"

/////////////////////////////// Inicializacion de variables globales ///////////////////////////////
int fd_memoria;
int fd_kernel;
int fd_cpu;
extern t_log* logger;
extern t_config_memoria* cfg;
extern t_sistema_memoria* sistema_memoria;
int cpus_conectadas = 0;

typedef struct {
    int fd;
    char* server_name;
} t_procesar_conexion_args;

// Declaración de funciones internas
void* leer_pagina_completa(int pid, int direccion_base_pagina);

int iniciar_conexiones_memoria(char* PUERTO_ESCUCHA, t_log* logger_param) {
    if (logger_param == NULL) {
        printf("Error: logger no inicializado\n");
        exit(EXIT_FAILURE);
    }

    fd_memoria = iniciar_servidor(PUERTO_ESCUCHA, logger_param, "Memoria iniciado");

    if (fd_memoria == -1) {
        log_error(logger_param, "No se pudo iniciar el servidor de Memoria");
        exit(EXIT_FAILURE);
    }

    log_trace(logger_param, "Esperando conexiones entrantes en Memoria...");
    return fd_memoria; // Devuelve el socket del servidor
}

// OPCIÓN A: Usar lista de threads para hacer join al finalizar
static t_list* threads_activos = NULL;

int server_escuchar(char* server_name, int server_socket) {
    if (!threads_activos) {
        threads_activos = list_create();
    }
    
    int cliente_socket = esperar_cliente(server_socket, logger);
    if (cliente_socket != -1) {
        pthread_t* hilo = malloc(sizeof(pthread_t));
        t_procesar_conexion_args* args = malloc(sizeof(t_procesar_conexion_args));
        args->fd = cliente_socket;
        args->server_name = server_name;
        
        if (pthread_create(hilo, NULL, (void*) procesar_conexion, (void*) args) != 0) {
            log_error(logger, "Error al crear hilo para procesar conexión de %s (fd=%d)", server_name, cliente_socket);
            // free(hilo);
            // free(args);
            close(cliente_socket);
            exit(EXIT_FAILURE);
        }
        
        // Guardar thread para hacer join después
        list_add(threads_activos, hilo);
        
        return 1;
    }
    return 0;
}

// Función para limpiar threads al finalizar sistema
void finalizar_threads_servidor() {
    if (threads_activos) {
        for (int i = 0; i < list_size(threads_activos); i++) {
            pthread_t* hilo = list_get(threads_activos, i);
            pthread_join(*hilo, NULL);
            free(hilo);
        }
        list_destroy(threads_activos);
        threads_activos = NULL;
    }
}

void procesar_conexion(void* void_args) {
    t_procesar_conexion_args* args = (t_procesar_conexion_args*) void_args;
    if (args == NULL) {
        log_trace(logger, "Error: Argumentos de conexion nulos");
        return;
    }

    int cliente_socket = args->fd;
    char* server_name = args->server_name;
    free(args);

    int handshake = -1;
    if (recv(cliente_socket, &handshake, sizeof(int), 0) <= 0) {
        log_trace(logger, "Error al recibir handshake del cliente (fd=%d): %s", cliente_socket, strerror(errno));
        close(cliente_socket);
        return;
    }
    bool es_cpu = false;  
    switch (handshake) {
        case HANDSHAKE_MEMORIA_KERNEL:
            log_trace(logger, AMARILLO("## Kernel Conectado - FD del socket: %d"), cliente_socket);
            fd_kernel = cliente_socket;
            break;

        case HANDSHAKE_MEMORIA_CPU:
            log_trace(logger, VERDE("## CPU Conectado - FD del socket: %d"), cliente_socket);
            es_cpu = true;  
            cpus_conectadas++;
            fd_cpu = cliente_socket; // Se sobreescribe fd_cpu si hay múltiples CPUs?
            break;

        default:
            log_trace(logger, "Handshake invalido recibido (fd=%d): %d", cliente_socket, handshake);
            close(cliente_socket);
            return;
    }
    log_trace(logger, "Conexion procesada exitosamente para %s (fd=%d)", server_name, cliente_socket);

    op_code cop;
    while ( (cop = recibir_operacion(cliente_socket)) != -1 ) {
        procesar_cod_ops(cop, cliente_socket);
    }

    log_trace(logger, "El cliente (fd=%d) se desconectó de %s", cliente_socket, server_name);

    close(cliente_socket);

    if (es_cpu) {
        cpus_conectadas--;

        if (cpus_conectadas == 0) {
            log_info(logger, "Se desconectaron todas las CPU. Finalizando Memoria...");
            // finalizar_sistema_memoria();
            exit(EXIT_SUCCESS);
        }
    }
}

void procesar_cod_ops(op_code cop, int cliente_socket) {
    switch (cop) {
        case WRITE_OP: {
            log_trace(logger, "WRITE_OP recibido");
            procesar_write_op(cliente_socket);
            break;
        }
        case READ_OP: {
            log_trace(logger, "READ_OP recibido");
            procesar_read_op(cliente_socket);
            break;
        }
        case INIT_PROC_OP: {
            log_trace(logger, "[INIT_PROC_OP] INIT_PROC_OP recibido");

            // ========== RECIBIR PARÁMETROS ==========
            t_list* lista = recibir_contenido_paquete(cliente_socket);
            int pid = *(int*)list_get(lista, 0); // PID
            char* nombre_proceso = strdup((char*)list_get(lista, 1)); // Nombre del proceso
            int tamanio = *(int*)list_get(lista, 2); // Size del proceso en memoria
            
            log_info(logger, AMARILLO("## Kernel solicita INIT_PROC para PID: %d"), pid);
            log_trace(logger, "[INIT_PROC_OP] Inicialización de proceso solicitada - PID: %d, Tamaño: %d, Nombre: '%s'", pid, tamanio, nombre_proceso);

            // ========== EJECUCIÓN DEL PROCESO DE CREACIÓN ==========
            t_resultado_memoria resultado = crear_proceso_en_memoria(pid, tamanio, nombre_proceso);
            
            // ========== CARGA DE INSTRUCCIONES ==========
            if (resultado == MEMORIA_OK) {
                
                // Construir el path completo usando PATH_INSTRUCCIONES de la configuración
                char* path_completo = string_from_format("%s%s", cfg->PATH_INSTRUCCIONES, nombre_proceso);
                log_trace(logger, "[INIT_PROC_OP] NOMBRE DEL PATH '%s'", path_completo);
            
                t_process_instructions* instrucciones = load_process_instructions(pid, path_completo);
                if (instrucciones != NULL) {
                    char* pid_key = string_itoa(pid);
                    dictionary_put(sistema_memoria->process_instructions, pid_key, instrucciones);
                    log_trace(logger, "[INIT_PROC_OP] PID: %d - Instrucciones cargadas desde %s", pid, path_completo);
                    free(pid_key);
                } else {
                    log_trace(logger, "[INIT_PROC_OP] PID: %d - No se pudieron cargar instrucciones desde %s", pid, path_completo);
                }
            
                free(path_completo);
            }

            // ========== LIBERACIÓN DE MEMORIA TEMPORAL ==========
            free(nombre_proceso);
            list_destroy_and_destroy_elements(lista, free);

            // ========== ENVÍO DE RESPUESTA A KERNEL ==========
            t_respuesta respuesta = (resultado == MEMORIA_OK) ? OK : ERROR;
            
            if (resultado == MEMORIA_OK) {
                log_trace(logger, "[INIT_PROC_OP] Enviando respuesta OK a cliente (fd=%d) - Proceso %d creado exitosamente", cliente_socket, pid);
            } else {
                log_trace(logger, "[INIT_PROC_OP] Enviando respuesta ERROR a cliente (fd=%d) - Falló creación del proceso %d", cliente_socket, pid);
            }
            // aplicar retardo memoria?
            send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);
            break;
        }
        case DUMP_MEMORY_OP: {
            log_trace(logger, "DUMP_MEMORY_OP recibido");

            t_list* parametros = recibir_contenido_paquete(cliente_socket);
            int pid = *(int*)list_get(parametros, 0);
            
            log_info(logger, AMARILLO("## Kernel solicita DUMP_MEMORY para PID: %d"), pid);

            // Ejecutar el dump
            t_resultado_memoria resultado = procesar_memory_dump(pid);

            // Enviar respuesta a Kernel
            t_respuesta respuesta = (resultado == MEMORIA_OK) ? OK : ERROR;
            log_trace(logger, "Enviando respuesta %s a cliente (fd=%d)", (respuesta == OK) ? "OK" : "ERROR", cliente_socket);
            // aplicar retardo memoria?
            send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);

            list_destroy_and_destroy_elements(parametros, free);
            break;
        }
        case FINALIZAR_PROC_OP: {
            log_trace(logger, "FINALIZAR_PROC_OP recibido (paquete)");
            t_list* lista = recibir_contenido_paquete(cliente_socket);
            if (!lista || list_size(lista) < 1) {
                log_trace(logger, "FINALIZAR_PROC_OP: Error al recibir paquete o paquete vacío");
                if (lista) list_destroy_and_destroy_elements(lista, free);
                return;
            }
            int pid = *(int*)list_get(lista, 0);
            log_info(logger, AMARILLO("## Kernel solicita FINALIZAR_PROC para PID: %d"), pid);
            log_trace(logger, "FINALIZAR_PROC_OP: PID recibido en paquete: %d", pid);
            list_destroy_and_destroy_elements(lista, free);
            log_trace(logger, "Finalización de proceso solicitada - PID: %d", pid);
            log_trace(logger, "FINALIZAR_PROC_OP: Llamando a finalizar_proceso_en_memoria para PID %d", pid);
            t_resultado_memoria resultado = finalizar_proceso_en_memoria(pid);
            log_trace(logger, "FINALIZAR_PROC_OP: finalizar_proceso_en_memoria retornó %d para PID %d", resultado, pid);
            t_respuesta respuesta = (resultado == MEMORIA_OK) ? OK : ERROR;
            log_trace(logger, "FINALIZAR_PROC_OP: Preparando envío de respuesta %s a cliente (fd=%d)", (respuesta == OK) ? "OK" : "ERROR", cliente_socket);
            // aplicar retardo memoria?
            int bytes_sent = send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);
            log_trace(logger, "FINALIZAR_PROC_OP: Respuesta enviada (%d bytes) exitosamente", bytes_sent);
            break;
        }
        case PEDIR_INSTRUCCION_OP: {
            log_trace(logger, "PEDIR_INSTRUCCION_OP recibido");

            // ========== RECIBIR PARÁMETROS ==========
            t_list* parametros_pedir_instruccion = recibir_contenido_paquete(cliente_socket);
            if (!parametros_pedir_instruccion) {
                log_trace(logger, "[PEDIR_INSTRUCCION] Error al recibir parámetros del paquete - socket: %d", cliente_socket);
                return;
            }
            if (list_size(parametros_pedir_instruccion) < 2) {
                log_trace(logger, "[PEDIR_INSTRUCCION] Parámetros insuficientes recibidos - socket: %d", cliente_socket);
                list_destroy_and_destroy_elements(parametros_pedir_instruccion, free);
                return;
            }
            
            // Recibo PID y PC
            int pid = *(int*)list_get(parametros_pedir_instruccion, 0);  // PID
            int pc = *(int*)list_get(parametros_pedir_instruccion, 1);   // PC
            list_destroy_and_destroy_elements(parametros_pedir_instruccion, free);

            log_trace(logger, "Instrucción solicitada - PID: %d, PC: %d - Socket: %d", pid, pc, cliente_socket);
            
            // ========== OBTENER INSTRUCCIÓN ==========
            t_instruccion* instruccion = get_instruction(pid, pc);
            
            if (instruccion != NULL) {
                // Log obligatorio
                log_instruccion_obtenida(pid, pc, instruccion);
                
                // Enviar instrucción usando paquetes
                enviar_instruccion_a_cpu(cliente_socket, pid, pc,
                                       instruccion->parametros1, 
                                       instruccion->parametros2, 
                                       instruccion->parametros3);
                
                // Liberar la instrucción obtenida
                liberar_instruccion(instruccion);
            } else {
                // Enviar error usando paquetes (strings vacíos)
                enviar_instruccion_a_cpu(cliente_socket, pid, pc, "", "", "");
                log_trace(logger, "[PEDIR_INSTRUCCION] Error: instrucción no encontrada - PID: %d, PC: %d", pid, pc);
            }
            break;
        }
        case PEDIR_CONFIG_CPU_OP: {
            log_trace(logger, "PEDIR_CONFIG_CPU_OP recibido");

            // Enviar la configuración necesaria para la CPU
                int entradas_por_tabla = cfg->ENTRADAS_POR_TABLA;
                int tam_pagina = cfg->TAM_PAGINA;
                int cantidad_niveles = cfg->CANTIDAD_NIVELES;
            
            // Enviamos los valores
                send_data(cliente_socket, &entradas_por_tabla, sizeof(int));
                send_data(cliente_socket, &tam_pagina, sizeof(int));
                send_data(cliente_socket, &cantidad_niveles, sizeof(int));
                
                log_trace(logger, "Configuración enviada a CPU: Entradas por tabla: %d, Tamaño página: %d, Niveles: %d",
                        entradas_por_tabla, tam_pagina, cantidad_niveles);
            break;
        }

        // ============================================================================
        // HANDLERS PARA LOS 4 TIPOS DE ACCESO ESPECÍFICOS DE MEMORIA
        // ============================================================================
        case ACCESO_TABLA_PAGINAS_OP: {
            log_trace(logger, "ACCESO_TABLA_PAGINAS_OP recibido");

            // Recibir parámetros: PID y número de página
            t_list* lista = recibir_contenido_paquete(cliente_socket);
            int pid = *(int*)list_get(lista, 0);
            int numero_pagina = *(int*)list_get(lista, 1);
            list_destroy_and_destroy_elements(lista, free);
            
            log_trace(logger, "Acceso a tabla de páginas - PID: %d, Página: %d", pid, numero_pagina);
            
            // Realizar acceso a tabla de páginas
            int numero_marco = acceso_tabla_paginas(pid, numero_pagina);
            
            // Enviar respuesta
            if (numero_marco != -1) {
                t_paquete* paquete_respuesta = crear_paquete_op(PAQUETE_OP);
                agregar_entero_a_paquete(paquete_respuesta, numero_marco);
                enviar_paquete(paquete_respuesta, cliente_socket);
                eliminar_paquete(paquete_respuesta);
                log_trace(logger, "Marco %d enviado para PID: %d, Página: %d", numero_marco, pid, numero_pagina);
            } else {
                t_paquete* paquete_respuesta = crear_paquete_op(PAQUETE_OP);
                int error = -1;
                agregar_entero_a_paquete(paquete_respuesta, error);
                enviar_paquete(paquete_respuesta, cliente_socket);
                eliminar_paquete(paquete_respuesta);
                log_trace(logger, "Error en acceso a tabla de páginas - PID: %d, Página: %d", pid, numero_pagina);
            }
            break;
        }
        case ACCESO_ESPACIO_USUARIO_OP: {
            log_trace(logger, "ACCESO_ESPACIO_USUARIO_OP recibido");

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
            
            log_trace(logger, "Acceso a espacio de usuario - PID: %d, Dir: %d, Tamaño: %d, Escritura: %s", 
                     pid, direccion_fisica, tamanio, es_escritura ? "true" : "false");
            
            if (es_escritura) {
                // Para escritura, recibir datos adicionales
                void* datos = list_get(lista, 4);
                bool resultado = acceso_espacio_usuario_escritura(pid, direccion_fisica, tamanio, datos);
                
                // Enviar respuesta
                t_respuesta respuesta = resultado ? OK : ERROR;
                send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);
                
                log_trace(logger, "Escritura en espacio de usuario %s - PID: %d", 
                         resultado ? "exitosa" : "fallida", pid);
            } else {
                // Para lectura, devolver datos
                void* datos = acceso_espacio_usuario_lectura(pid, direccion_fisica, tamanio);
                
                if (datos != NULL) {
                    send_data(cliente_socket, datos, tamanio);
                    free(datos);
                    log_trace(logger, "Lectura de espacio de usuario exitosa - PID: %d", pid);
                } else {
                    t_respuesta respuesta = ERROR;
                    send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);
                    log_trace(logger, "Error en lectura de espacio de usuario - PID: %d", pid);
                }
            }
            
            list_destroy_and_destroy_elements(lista, free);
            break;
        }
        case LEER_PAGINA_COMPLETA_OP: {
            log_trace(logger, "LEER_PAGINA_COMPLETA_OP recibido");

            // Recibir parámetros del paquete: PID y dirección base de página
            t_list* lista = recibir_contenido_paquete(cliente_socket);
            if (lista == NULL || list_size(lista) < 2) {
                log_trace(logger, "LEER_PAGINA_COMPLETA_OP: Parámetros insuficientes");
                if (lista) list_destroy_and_destroy_elements(lista, free);
                return;
            }
            
            int pid = *(int*)list_get(lista, 0);
            int direccion_base_pagina = *(int*)list_get(lista, 1);
            list_destroy_and_destroy_elements(lista, free);
            
            log_trace(logger, "Leer página completa - PID: %d, Dir. Base Página: %d", pid, direccion_base_pagina);
            
            // Leer página completa
            void* pagina_completa = leer_pagina_completa(pid, direccion_base_pagina);
            
            if (pagina_completa != NULL) {
                // Enviar página completa como paquete
                t_paquete* paquete_respuesta = crear_paquete_op(PAQUETE_OP);
                agregar_a_paquete(paquete_respuesta, pagina_completa, cfg->TAM_PAGINA);
                enviar_paquete(paquete_respuesta, cliente_socket);
                eliminar_paquete(paquete_respuesta);
                free(pagina_completa);
                log_trace(logger, "Página completa enviada como paquete - PID: %d, Dir: %d", pid, direccion_base_pagina);
            } else {
                // Enviar error como paquete
                t_paquete* paquete_respuesta = crear_paquete_op(PAQUETE_OP);
                int error = -1;
                agregar_entero_a_paquete(paquete_respuesta, error);
                enviar_paquete(paquete_respuesta, cliente_socket);
                eliminar_paquete(paquete_respuesta);
                log_info(logger, "Error al leer página completa - PID: %d, Dir: %d", pid, direccion_base_pagina);
            }
            break;
        }
        case ACTUALIZAR_PAGINA_COMPLETA_OP: {
            log_trace(logger, "ACTUALIZAR_PAGINA_COMPLETA_OP recibido");

            // Recibir parámetros del paquete
            t_list* lista = recibir_contenido_paquete(cliente_socket);
            
            char* pid_str = (char*)list_get(lista, 0);
            char* direccion_str = (char*)list_get(lista, 1);
            
            int pid = atoi(pid_str);
            int direccion_fisica = atoi(direccion_str);
            
            // El contenido de la página viene como el tercer elemento
            void* contenido_pagina = list_get(lista, 2);
            
            log_trace(logger, "Actualizar página completa - PID: %d, Dir. Física: %d", pid, direccion_fisica);
            
            // Actualizar página completa
            bool resultado = actualizar_pagina_completa(pid, direccion_fisica, contenido_pagina);
            
            // Enviar respuesta
            t_respuesta respuesta = resultado ? OK : ERROR;
            send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);
            
            log_trace(logger, "Actualización de página completa %s - PID: %d", 
                     resultado ? "exitosa" : "fallida", pid);
            
            list_destroy_and_destroy_elements(lista, free);
            break;
        }
        case CHECK_MEMORY_SPACE_OP: {
            log_trace(logger, "CHECK_MEMORY_SPACE_OP recibido");

            // Recibir tamaño solicitado
            t_list* datos = recibir_contenido_paquete(cliente_socket);
            if (list_size(datos) < 1) {
                log_trace(logger, "CHECK_MEMORY_SPACE_OP: No se recibieron datos válidos");
                list_destroy_and_destroy_elements(datos, free);
                break;
            }

            int tamanio;
            tamanio = *(int*)list_get(datos, 0);
            list_destroy_and_destroy_elements(datos, free);
            
            log_info(logger, AMARILLO("## Kernel solicita CHECK_MEMORY_SPACE para tamaño: %d"), tamanio);

            // Verificar espacio disponible
            bool hay_espacio = verificar_espacio_disponible(tamanio);
            
            // Enviar respuesta
            t_respuesta respuesta = hay_espacio ? OK : ERROR;
            send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);
            
            log_trace(logger, "Respuesta de verificación de espacio: %s", 
                     hay_espacio ? "OK" : "ERROR");
            break;
        }
        case SUSPENDER_PROCESO_OP: {
            log_trace(logger, "SUSPENDER_PROCESO_OP recibido");

            // Recibir paquete con el PID
            t_list* parametros = recibir_contenido_paquete(cliente_socket);
            if (!parametros || list_size(parametros) < 1) {
                log_trace(logger, "SUSPENDER_PROCESO_OP: Error al recibir paquete o paquete vacío");
                if (parametros) list_destroy_and_destroy_elements(parametros, free);
                return;
            }
            int pid = *(int*)list_get(parametros, 0);
            
            log_info(logger, AMARILLO("## Kernel solicita SUSPENDER_PROCESO para PID: %d"), pid);
            list_destroy_and_destroy_elements(parametros, free);

            log_trace(logger, "Suspensión de proceso solicitada - PID: %d", pid);

            t_resultado_memoria resultado = suspender_proceso_en_memoria(pid);

            if (resultado == MEMORIA_OK) {
                aplicar_retardo_swap();
            }    

            t_respuesta respuesta = (resultado == MEMORIA_OK) ? OK : ERROR;
            send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);

            log_trace(logger, "Suspensión de proceso %s - PID: %d",
                    (respuesta == OK) ? "exitosa" : "fallida", pid);
            break;
        }
        case DESUSPENDER_PROCESO_OP: {
            log_trace(logger, "DESUSPENDER_PROCESO_OP recibido");

            // Recibir paquete con el PID
            t_list* parametros = recibir_contenido_paquete(cliente_socket);
            if (!parametros || list_size(parametros) < 1) {
                log_trace(logger, "DESUSPENDER_PROCESO_OP: Error al recibir paquete o paquete vacío");
                if (parametros) list_destroy_and_destroy_elements(parametros, free);
                return;
            }
            int pid = *(int*)list_get(parametros, 0);
            
            log_info(logger, AMARILLO("## Kernel solicita DESUSPENDER_PROCESO para PID: %d"), pid);


            log_trace(logger, "Des-suspensión de proceso solicitada - PID: %d", pid);

            t_resultado_memoria resultado = reanudar_proceso_en_memoria(pid);

            if (resultado == MEMORIA_OK) {
                aplicar_retardo_swap();
            }            

            t_respuesta respuesta = (resultado == MEMORIA_OK) ? OK : ERROR;
            log_info(logger, NARANJA("## (%d) Confirmación de desuspensión enviada"), pid);
            list_destroy_and_destroy_elements(parametros, free);
            
            /*send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);*/

            log_trace(logger, "Des-suspensión de proceso %s - PID: %d",
                    (respuesta == OK) ? "exitosa" : "fallida", pid);
            break;
        }
        default: { 
            log_error(logger, "Codigo de operacion desconocido recibido del cliente %d: %d", cliente_socket, cop);
            break;
        }
    }
}

void procesar_write_op(int cliente_socket) {
    t_list* lista = recibir_contenido_paquete(cliente_socket);

    if (list_size(lista) != 4) {
        log_trace(logger, "WRITE_OP: Se esperaban 4 parámetros pero se recibieron %d", list_size(lista));
        list_destroy_and_destroy_elements(lista, free);
        return;
    }

    int pid = *(int*)list_get(lista, 0);
    int direccion_fisica = *(int*)list_get(lista, 1);
    int size = *(int*)list_get(lista, 2);
    void* datos_str = list_get(lista, 3);

    log_trace(logger, "## PID: %d - Escritura - Dir. Física: %d - Tamaño: %d", pid, direccion_fisica, size);

    if (direccion_fisica + size > cfg->TAM_MEMORIA) {
        log_trace(logger, "PID: %d - Escritura fuera de rango (dir=%d + size=%d > tam=%d)", pid, direccion_fisica, size, cfg->TAM_MEMORIA);
        t_respuesta err = ERROR;
        send(cliente_socket, &err, sizeof(t_respuesta), 0);
        list_destroy_and_destroy_elements(lista, free);
        return;
    }

    // Usar el parámetro size en lugar de strlen()
    memcpy((char*)(sistema_memoria->memoria_principal + direccion_fisica), datos_str, size);
    aplicar_retardo_memoria();

    t_respuesta respuesta = OK;
    send(cliente_socket, &respuesta, sizeof(t_respuesta), 0);

    list_destroy_and_destroy_elements(lista, free);
}

void procesar_read_op(int cliente_socket) {
    t_list* lista = recibir_contenido_paquete(cliente_socket);

    if (list_size(lista) != 3) {
        log_trace(logger, "READ_OP: Se esperaban 3 parámetros pero se recibieron %d", list_size(lista));
        list_destroy_and_destroy_elements(lista, free);
        return;
    }

    int direccion_fisica = *(int*)list_get(lista, 0);
    int size = *(int*)list_get(lista, 1);
    int pid = *(int*)list_get(lista, 2);

    log_trace(logger, "## PID: %d - Lectura - Dir. Física: %d - Tamaño: %d", pid, direccion_fisica, size);

    actualizar_metricas(pid, "MEMORY_READ");

    char* datos_leidos = malloc(size + 1);
    memcpy(datos_leidos, sistema_memoria->memoria_principal + direccion_fisica, size);
    datos_leidos[size] = '\0';

    char vista[64] = {0};
    int max_ver = size > 60 ? 60 : size;
    strncpy(vista, datos_leidos, max_ver);
    log_trace(logger, "READ - PID: %d - Bytes [%d:%d] = '%s'", pid, direccion_fisica, direccion_fisica + size - 1, vista);

    aplicar_retardo_memoria();
    
    // Enviar respuesta como un string en un paquete 
    t_paquete* paquete_respuesta = crear_paquete_op(PAQUETE_OP);
    agregar_a_paquete(paquete_respuesta, datos_leidos, size + 1);
    enviar_paquete(paquete_respuesta, cliente_socket);
    eliminar_paquete(paquete_respuesta);

    free(datos_leidos);
    list_destroy_and_destroy_elements(lista, free);
}