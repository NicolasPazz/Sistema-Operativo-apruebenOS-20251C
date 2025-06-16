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
                *((int*)leer_pagina(direccion)) = valor;
            
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

            // Recibir parámetros desde paquete
            t_list* lista = recibir_contenido_paquete(cliente_socket);
            
            int pid = *(int*)list_get(lista, 0);
            char* nombre_proceso = strdup((char*)list_get(lista, 1));  // Hacer una copia del string
            int tamanio = *(int*)list_get(lista, 2);
            
            log_debug(logger, "Inicialización de proceso solicitada - PID: %d, Tamaño: %d, Nombre: '%s'", pid, tamanio, nombre_proceso);
        
            // 1. Construir path completo para instrucciones
            char* path_completo = string_from_format("%s%s", cfg->PATH_INSTRUCCIONES, nombre_proceso);
            log_debug(logger, "Path de instrucciones: '%s'", path_completo);
            
            // 2. Verificar si el proceso ya existe
            char* pid_key = string_itoa(pid);
            if (dictionary_has_key(sistema_memoria->procesos, pid_key)) {
                log_error(logger, "PID: %d - El proceso ya existe en memoria", pid);
                free(path_completo);
                free(pid_key);
                free(nombre_proceso);  // Liberar la copia del nombre
                list_destroy_and_destroy_elements(lista, free);
                
                t_respuesta_memoria respuesta = ERROR;
                log_info(logger, "Enviando respuesta ERROR a cliente (fd=%d) - Proceso ya existe", cliente_socket);
                send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
                break;
            }
            
            // 3. Calcular páginas necesarias
            int paginas_necesarias = (tamanio + cfg->TAM_PAGINA - 1) / cfg->TAM_PAGINA;
            log_debug(logger, "PID: %d - Páginas necesarias: %d", pid, paginas_necesarias);
            
            // 4. Verificar espacio disponible en memoria principal
            pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);
            if (sistema_memoria->admin_marcos->frames_libres < paginas_necesarias) {
                log_error(logger, "PID: %d - No hay suficiente espacio en memoria (necesita %d páginas, hay %d libres)", 
                          pid, paginas_necesarias, sistema_memoria->admin_marcos->frames_libres);
                pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
                free(path_completo);
                free(pid_key);
                free(nombre_proceso);  // Liberar la copia del nombre
                list_destroy_and_destroy_elements(lista, free);
                
                t_respuesta_memoria respuesta = ERROR;
                log_info(logger, "Enviando respuesta ERROR a cliente (fd=%d) - No hay espacio suficiente", cliente_socket);
                send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
                break;
            }
            pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
            
            // 5. Crear proceso en memoria con estructuras administrativas completas
            t_proceso_memoria* proceso = crear_proceso_memoria(pid, tamanio);
            if (proceso == NULL) {
                log_error(logger, "PID: %d - Error al crear proceso en memoria", pid);
                free(path_completo);
                free(pid_key);
                free(nombre_proceso);  // Liberar la copia del nombre
                list_destroy_and_destroy_elements(lista, free);
                
                t_respuesta_memoria respuesta = ERROR;
                log_info(logger, "Enviando respuesta ERROR a cliente (fd=%d) - Error al crear proceso", cliente_socket);
                send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
                break;
            }
            
            // 6. Asignar frames para todas las páginas del proceso
            bool asignacion_exitosa = true;
            for (int i = 0; i < paginas_necesarias && asignacion_exitosa; i++) {
                int frame_asignado = asignar_frame_libre(pid, i);
                if (frame_asignado == -1) {
                    log_error(logger, "PID: %d - Error al asignar frame para página %d", pid, i);
                    asignacion_exitosa = false;
                    break;
                }
                
                // Actualizar entrada en tabla de páginas
                t_entrada_tabla* entrada = buscar_entrada_tabla(proceso->estructura_paginas, i);
                if (entrada != NULL) {
                    entrada->presente = true;
                    entrada->numero_frame = frame_asignado;
                    log_trace(logger, "PID: %d - Página %d asignada al frame %d", pid, i, frame_asignado);
                }
            }
            
            if (!asignacion_exitosa) {
                // Liberar frames ya asignados y proceso
                for (int i = 0; i < paginas_necesarias; i++) {
                    t_entrada_tabla* entrada = buscar_entrada_tabla(proceso->estructura_paginas, i);
                    if (entrada != NULL && entrada->presente) {
                        liberar_frame(entrada->numero_frame);
                    }
                }
                liberar_proceso_memoria(proceso);
                free(path_completo);
                free(pid_key);
                free(nombre_proceso);  // Liberar la copia del nombre
                list_destroy_and_destroy_elements(lista, free);
                
                t_respuesta_memoria respuesta = ERROR;
                send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
                break;
            }
            
            // 7. Agregar proceso a los diccionarios del sistema
            dictionary_put(sistema_memoria->procesos, pid_key, proceso);
            dictionary_put(sistema_memoria->estructuras_paginas, pid_key, proceso->estructura_paginas);
            dictionary_put(sistema_memoria->metricas_procesos, pid_key, proceso->metricas);
            
            // 8. Cargar instrucciones desde archivo
            t_process_instructions* instrucciones = load_process_instructions(pid, path_completo);
            if (instrucciones != NULL) {
                dictionary_put(sistema_memoria->process_instructions, pid_key, instrucciones);
                log_debug(logger, "PID: %d - Instrucciones cargadas desde %s", pid, path_completo);
            } else {
                log_warning(logger, "PID: %d - No se pudieron cargar instrucciones desde %s", pid, path_completo);
            }
            
            // 9. Actualizar estadísticas del sistema
            sistema_memoria->procesos_activos++;
            sistema_memoria->memoria_utilizada += tamanio;
            
            // 10. Liberar memoria temporal
            free(path_completo);
            free(pid_key);
            free(nombre_proceso);  // Liberar la copia del nombre
            list_destroy_and_destroy_elements(lista, free);
        
            // 11. Enviar respuesta de éxito
            t_respuesta_memoria respuesta = OK;
            log_info(logger, "Enviando respuesta OK a cliente (fd=%d)", cliente_socket);
            send(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0);
            
            log_info(logger, "PID: %d - Proceso inicializado exitosamente con %d páginas", pid, paginas_necesarias);
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
                t_respuesta_memoria respuesta = OK;
                log_info(logger, "Enviando respuesta OK a cliente (fd=%d)", cliente_socket);
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

            // CAMBIO: Recibir PID y PC desde paquete (para coincidir con CPU)
            t_list* lista = recibir_2_enteros_sin_op(cliente_socket);
            int pid = (int)(intptr_t)list_get(lista, 0);  // PID primero
            int pc = (int)(intptr_t)list_get(lista, 1);   // PC segundo
            list_destroy(lista);
            
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
            
            // Calcular páginas necesarias
            int paginas_necesarias = (tamanio + cfg->TAM_PAGINA - 1) / cfg->TAM_PAGINA;
            log_debug(logger, "Verificación de espacio - Tamaño: %d bytes, Páginas necesarias: %d", 
                     tamanio, paginas_necesarias);
            
            // Verificar espacio disponible
            pthread_mutex_lock(&sistema_memoria->admin_marcos->mutex_frames);
            bool hay_espacio = sistema_memoria->admin_marcos->frames_libres >= paginas_necesarias;
            pthread_mutex_unlock(&sistema_memoria->admin_marcos->mutex_frames);
            
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

t_proceso_memoria* crear_proceso_memoria(int pid, int tamanio) {
    t_proceso_memoria* proceso = malloc(sizeof(t_proceso_memoria));
    if (!proceso) {
        log_error(logger, "Error al crear proceso %d", pid);
        return NULL;
    }

    // Inicializar campos básicos
    proceso->pid = pid;
    proceso->tamanio = tamanio;
    proceso->activo = true;
    proceso->suspendido = false;
    proceso->timestamp_creacion = time(NULL);
    proceso->timestamp_ultimo_uso = time(NULL);

    // Crear estructura de páginas
    proceso->estructura_paginas = crear_estructura_paginas(pid, tamanio);
    if (!proceso->estructura_paginas) {
        log_error(logger, "Error al crear estructura de páginas para proceso %d", pid);
        free(proceso);
        return NULL;
    }

    // Crear métricas
    proceso->metricas = crear_metricas_proceso(pid);
    if (!proceso->metricas) {
        log_error(logger, "Error al crear métricas para proceso %d", pid);
        destruir_estructura_paginas(proceso->estructura_paginas);
        free(proceso);
        return NULL;
    }

    // Inicializar lista de instrucciones
    proceso->instrucciones = list_create();
    if (!proceso->instrucciones) {
        log_error(logger, "Error al crear lista de instrucciones para proceso %d", pid);
        destruir_metricas_proceso(proceso->metricas);
        destruir_estructura_paginas(proceso->estructura_paginas);
        free(proceso);
        return NULL;
    }

    return proceso;
}