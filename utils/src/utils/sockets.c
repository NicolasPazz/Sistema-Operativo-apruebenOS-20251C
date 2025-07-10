#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../headers/sockets.h"

        /////////////////////////////// Log y Config ///////////////////////////////
    t_log* iniciar_logger(char *file, char *process_name, bool is_active_console, t_log_level level) {
    remove(file);
	t_log* nuevo_logger = log_create(file, process_name,is_active_console,level);
	if (nuevo_logger == NULL) {
		perror("Error al crear el log");
		exit(EXIT_FAILURE);
	}
	return nuevo_logger;
}

t_config* iniciar_config(char* path) {
	t_config* nuevo_config = config_create(path);
	if (nuevo_config == NULL) {
		perror("Error al intentar leer el config");
		exit(EXIT_FAILURE);
	}
	return nuevo_config;
}

        /////////////////////////////// Conexiones ///////////////////////////////
int iniciar_servidor(char *puerto, t_log* logger, char* msj_server) {
	if (logger == NULL) {
        printf("Error: Logger no inicializado\n");
        return -1;
    }
	struct addrinfo hints, *servinfo;
	int optval = 1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, puerto, &hints, &servinfo) != 0) {
		log_error(logger, "%s: error en getaddrinfo para el puerto %s", msj_server, puerto);
		return -1;
	}

	// Crear socket
	int socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if (socket_servidor < 0) {
		log_error(logger, "%s: error al crear un socket servidor para el puerto %s: %s", msj_server, puerto, strerror(errno));
		freeaddrinfo(servinfo);
		return -1;
	}

	// Aplicar SO_REUSEADDR
	if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		log_error(logger, "%s: error al aplicar SO_REUSEADDR para el puerto %s: %s", msj_server, puerto, strerror(errno));
		close(socket_servidor);
		freeaddrinfo(servinfo);
		return -1;
	}

	// Bind y Listen
	if (bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
		log_error(logger, "%s: error en bind para el puerto %s: %s", msj_server, puerto, strerror(errno));
		close(socket_servidor);
		freeaddrinfo(servinfo);
		return -1;
	}

	if (listen(socket_servidor, SOMAXCONN) < 0) {
		log_error(logger, "%s: error en listen para el puerto %s: %s", msj_server, puerto, strerror(errno));
		close(socket_servidor);
		freeaddrinfo(servinfo);
		return -1;
	}

	freeaddrinfo(servinfo);
	log_trace(logger, AZUL("[Servidor]")VERDE(" %s")" escuchando en puerto"VERDE(" %s"), msj_server, puerto);

	return socket_servidor;
}

int crear_conexion(char *ip, char* puerto, t_log* logger) {
    struct addrinfo hints;
    struct addrinfo *server_info;
    int socket_cliente;
    int optval = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(ip, puerto, &hints, &server_info) != 0) {
        log_error(logger, "crear_conexion: Error en getaddrinfo para %s:%s", ip, puerto);
        return -1;
    }

    socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (socket_cliente < 0) {
        log_error(logger, "crear_conexion: Error al crear socket para %s:%s", ip, puerto);
        freeaddrinfo(server_info);
        return -1;
    }

    if (setsockopt(socket_cliente, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        log_error(logger, "crear_conexion: Error al aplicar SO_REUSEADDR para %s:%s", ip, puerto);
        close(socket_cliente);
        freeaddrinfo(server_info);
        return -1;
    }

    if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        log_error(logger, "crear_conexion: No se pudo conectar a %s:%s", ip, puerto);
        close(socket_cliente);
        freeaddrinfo(server_info);
        return -1;
    }

    freeaddrinfo(server_info);
    return socket_cliente;
}

int esperar_cliente(int socket_servidor, t_log* logger) {
	if (logger == NULL) {
        printf("Error: Logger no inicializado en esperar_cliente\n");
        return -1;
    }
    int socket_cliente = accept(socket_servidor, NULL, NULL);
    if (socket_cliente == -1) {
        log_error(logger, "Error al aceptar cliente: %s", strerror(errno));
        return -1;
    }
    log_trace(logger, "Se conecto un nuevo cliente (fd = %d)", socket_cliente);
    return socket_cliente;
}

void atender_cliente(void* arg) {
    cliente_data_t *data = (cliente_data_t *)arg;
    int control_key = 1;
    while (control_key) {
        int cod_op = recibir_operacion(data->fd);
        switch (cod_op) {
            case MENSAJE_OP:
                recibir_mensaje(data->fd, data->logger);
                break;
            case PAQUETE_OP:
                t_list* lista = recibir_paquete(data->fd);
                list_iterate(lista, (void*)iterator);
                list_destroy(lista);
                break;
            case -1:
                log_error(data->logger, "El cliente (%s) se desconecto. Terminando servidor.", data->cliente);
                control_key = 0;
                break;
            default:
                log_error(data->logger, "Operacion desconocida de %s", data->cliente);
                break;
        }
    }
    liberar_cliente_data(data);
}

/**
 * @brief Libera toda la memoria asociada a un cliente_data_t.
 *
 * Esta función libera la memoria heap alocada para la cadena 'cliente'
 * y luego para la estructura cliente_data_t completa.
 * 
 * @param data Puntero a la estructura cliente_data_t a liberar.
 */
void liberar_cliente_data(cliente_data_t *data) {
    if (data != NULL) {
        free(data->cliente);  // liberar la cadena duplicada
        free(data);           // liberar la estructura
    }
}

void liberar_conexion(int socket_cliente) {
	close(socket_cliente);
}

bool validar_handshake(int fd, handshake_code esperado, t_log* log) {
    int recibido;
    int ret = recv(fd, &recibido, sizeof(int), MSG_WAITALL);
    if (ret != sizeof(int)) {
        if (ret == 0) {
            log_error(log, "Error recibiendo handshake (fd=%d): conexión cerrada por el peer", fd);
        } else {
            log_error(log, "Error recibiendo handshake (fd=%d): %s", fd, strerror(errno));
        }
        return false;
    }

    if (recibido != esperado) {
        log_error(log, "Handshake invalido (fd=%d): se esperaba %d, se recibio %d", fd, esperado, recibido);
        return false;
    }

    return true;
}

        /////////////////////////////// Serializacion ///////////////////////////////
/**
 * @brief Serializa un paquete en un bloque continuo de memoria para su envío por red.
 *
 * Esta función toma un paquete que contiene un código de operación y un buffer de datos, 
 * y lo convierte en un único bloque de memoria contiguo con el siguiente formato:
 * 
 *     [op_code (int)][tamaño del buffer (int)][contenido del buffer (bytes)]
 *
 * Este orden debe ser respetado por el receptor para poder deserializar correctamente el paquete.
 *
 * @param paquete Puntero al paquete a serializar. Debe contener el código de operación y un buffer válido.
 * @param bytes Tamaño total del bloque a serializar. Debe ser igual a:
 *              paquete->buffer->size + 2 * sizeof(int)
 *
 * @return Puntero a un bloque de memoria contiguo listo para ser enviado con send().
 *         La memoria debe ser liberada por quien llama a la función.
 */
// WARNING: posible leak usando esta funcion (reserva memoria y no la libera), siempre liberar los mallocs despues de usar
void* serializar_paquete(t_paquete* paquete, int bytes) {
	void * magic = malloc(bytes);
    if (magic == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }

	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

// WARNING: posible leak usando esta funcion (reserva memoria y no la libera), siempre liberar los mallocs despues de usar
cliente_data_t *crear_cliente_data(int fd_cliente, t_log* logger, char* cliente) {
    cliente_data_t *data = malloc(sizeof(cliente_data_t));
    if (data == NULL) {
        perror("Error al asignar memoria para cliente_data_t*");
        exit(EXIT_FAILURE);
    }
    data->fd = fd_cliente;
    data->logger = logger;
    data->cliente = strdup(cliente);
    if (data->cliente == NULL) {
        perror("Error al asignar memoria para cliente en crear_cliente_data");
        free(data);
        exit(EXIT_FAILURE);
    }
    return data;
}

void paquete(int conexion) {
	char* leido;
	t_paquete* paquete = crear_paquete();

	leido = readline(">> ");
	
	while (strcmp(leido, "") != 0) {
		agregar_a_paquete(paquete,leido,strlen(leido)+1);
		free(leido);
		leido = readline(">> ");
	}

	enviar_paquete(paquete,conexion);
	free(leido);
	eliminar_paquete(paquete);
}

        /////////////////////////////// Envio de paquete/mensaje ///////////////////////////////
void enviar_mensaje(char* mensaje, int socket_cliente) {
	t_paquete* paquete = malloc(sizeof(t_paquete));
    if (paquete == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }

	paquete->codigo_operacion = MENSAJE_OP;
	paquete->buffer = malloc(sizeof(t_buffer));
    if (paquete->buffer == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
    if (paquete->buffer->stream == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

    if (send(socket_cliente, a_enviar, bytes, 0) != bytes) {
        perror("Error al enviar paquete");
    }
    
	free(a_enviar);
	eliminar_paquete(paquete);
}

/**
 * @brief Inicializa el buffer de un paquete asignando memoria vacía y seteando tamaño cero.
 * 
 * @param paquete Puntero al paquete cuyo buffer se va a inicializar.
 */
void crear_buffer(t_paquete* paquete) {
	paquete->buffer = malloc(sizeof(t_buffer));
    if (paquete->buffer == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

// WARNING: posible leak usando esta funcion (reserva memoria y no la libera), siempre liberar los mallocs despues de usar
t_paquete* crear_paquete(void) {
	t_paquete* paquete = malloc(sizeof(t_paquete));
    if (paquete == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }
	paquete->codigo_operacion = PAQUETE_OP;
	crear_buffer(paquete);
	return paquete;
}

/**
 * @brief Agrega un dato genérico al final del stream del paquete, precedido por su tamaño.
 * 
 * Este método permite agregar strings u otros datos dinámicos al buffer. El formato agregado es:
 * [int tamaño][bytes del dato]
 * 
 * @param paquete Puntero al paquete al que se le agregará el dato.
 * @param valor Puntero al contenido del dato a agregar.
 * @param tamanio Cantidad de bytes del dato (sin incluir el sizeof(int) agregado).
 */
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio) {
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

    memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
    memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

    paquete->buffer->size += tamanio + sizeof(int);
}

/**
 * @brief Serializa y envía un paquete completo por el socket especificado.
 * 
 * El mensaje enviado incluye: [op_code][tamaño buffer][buffer serializado]
 * 
 * @param paquete Puntero al paquete a enviar.
 * @param socket_cliente Descriptor del socket destino.
 */
void enviar_paquete(t_paquete* paquete, int socket_cliente) {
	int bytes = paquete->buffer->size + 2 * sizeof(int);    // 2 int, uno de op_code, otro de buffer->size
	void* a_enviar = serializar_paquete(paquete, bytes);

    if (send(socket_cliente, a_enviar, bytes, 0) != bytes) {
        perror("Error al enviar paquete");
    }
    
	free(a_enviar);
}

/**
 * @brief Libera toda la memoria asociada al paquete, incluyendo su buffer y stream.
 * 
 * @param paquete Puntero al paquete a liberar.
 */
void eliminar_paquete(t_paquete* paquete) {
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

/**
 * @brief Crea un nuevo paquete con un código de operación específico.
 * 
 * @param codop Código de operación que representará al paquete.
 * @return Puntero a un nuevo t_paquete con buffer inicializado con size 0 y stream null.
 */
// WARNING: posible leak usando esta funcion (reserva memoria y no la libera), siempre liberar los mallocs despues de usar
t_paquete* crear_paquete_op(op_code codop) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    if (paquete == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }
    paquete->codigo_operacion = codop;
    crear_buffer(paquete);
    return paquete;
}

/**
 * @brief Agrega un número entero al final del stream del paquete.
 * 
 * @param paquete Puntero al paquete al que se desea agregar el entero.
 * @param numero Valor entero (4 bytes) que se agregará al final del buffer.
 */
void agregar_entero_a_paquete(t_paquete *paquete, int numero) {
    int tamanio = sizeof(int);
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + sizeof(int) + tamanio);
    memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
    memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), &numero, tamanio);
    paquete->buffer->size += sizeof(int) + tamanio;
}

void agregar_string_a_paquete(t_paquete* paquete, char* cadena) {
    int longitud = strlen(cadena) + 1;
    agregar_a_paquete(paquete, cadena, longitud);
}

void agregar_entero_con_tamanio_a_paquete(t_paquete *paquete, int numero) {
    int param_size = sizeof(int);

    // Reservar espacio y agregar primero el tamaño
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + sizeof(int) + param_size);

    memcpy(paquete->buffer->stream + paquete->buffer->size, &param_size, sizeof(int));
    paquete->buffer->size += sizeof(int);

    // Luego el valor
    memcpy(paquete->buffer->stream + paquete->buffer->size, &numero, param_size);
    paquete->buffer->size += param_size;
}

bool enviar_operacion(int socket, op_code operacion) {
    int op = operacion;
    int bytes_enviados = send(socket, &op, sizeof(op), 0);
    return bytes_enviados == sizeof(op);
}

        /////////////////////////////// Deserializacion ///////////////////////////////
char* leer_string(char* buffer, int* desplazamiento) {
    int tamanio = leer_entero(buffer, desplazamiento);

    if (tamanio < 0 || tamanio > MAX_STRING_SIZE) {
        fprintf(stderr, "leer_string: Tamaño inválido (%d)\n", tamanio);
        exit(EXIT_FAILURE);
    }
    
    // Manejar string vacío
    if (tamanio == 0) {
        char* palabra = malloc(1);
        if (!palabra) {
            perror("leer_string: Error al asignar memoria");
            exit(EXIT_FAILURE);
        }
        palabra[0] = '\0';
        return palabra;
    }
    
    char* palabra = malloc(tamanio + 1);
    if (!palabra) {
        perror("leer_string: Error al asignar memoria");
        exit(EXIT_FAILURE);
    }

    memcpy(palabra, buffer + *desplazamiento, tamanio);

    palabra[tamanio] = '\0';

    *desplazamiento += tamanio;

    return palabra;
}

int leer_entero(char *buffer, int * desplazamiento) {
    int entero;
    memcpy(&entero, buffer + (*desplazamiento), sizeof(int));
    (*desplazamiento) += sizeof(int);
    return entero;
}

bool enviar_enteros(int socket, int* enteros, int cantidad) {
    int total_bytes = cantidad * sizeof(int);
    int enviados = send(socket, enteros, total_bytes, 0);
    return enviados == total_bytes;
}


        /////////////////////////////// Recepcion de paquete/mensaje ///////////////////////////////
/**
 * @brief Recibe el código de operación enviado desde un socket.
 * 
 * @param socket_cliente El descriptor del socket del cual se recibe el código.
 * @return op_code El código de operación recibido, o -1 si la conexión se cerró.
 */
op_code recibir_operacion(int socket_cliente) {
	op_code cod_op;
	if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) != sizeof(int)) {
        return -1;  // Retornar -1 para que lo maneje el caller
    }
    return cod_op;
}

/**
 * @brief Recibe un buffer desde el socket con tamaño dinámico.
 * 
 * @param size Puntero a entero donde se almacenará el tamaño del buffer recibido.
 * @param socket_cliente Socket desde el que se recibe el buffer.
 * @return void* Puntero al buffer recibido, debe liberarse manualmente.
 */
void* recibir_buffer(int* size, int socket_cliente) {
    void * buffer;

    // Leer el tamaño del buffer (2do int en el paquete)
    if (recv(socket_cliente, size, sizeof(int), MSG_WAITALL) != sizeof(int)) exit(EXIT_FAILURE);
    buffer = malloc(*size);
    if (buffer == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }

    if (recv(socket_cliente, buffer, *size, MSG_WAITALL) != *size) {
        free(buffer);
        exit(EXIT_FAILURE);
    }

    return buffer;
}

/**
 * @brief Recibe un mensaje de texto y lo muestra por log.
 * 
 * @param socket_cliente Socket desde el cual se recibe el mensaje.
 * @param logger Logger utilizado para mostrar el mensaje recibido.
 */
void recibir_mensaje(int socket_cliente,t_log* logger) {
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	log_trace(logger, "Me llego el mensaje: %s", buffer);
	free(buffer);
}

/**
 * @brief Recibe un paquete compuesto por múltiples parámetros serializados y los deserializa.
 * Este paquete debe estar compuesto por una serie de [int tamaño][contenido].
 * 
 * @param socket_cliente Socket desde el cual se recibe el paquete.
 * @return t_list* Lista de punteros a los parámetros deserializados. Cada elemento debe ser liberado manualmente.
 */
// WARNING: posible leak usando esta funcion (reserva memoria y no la libera), siempre liberar los mallocs despues de usar
t_list* recibir_contenido_paquete(int socket_cliente) {
    int buffer_size;

    // Recibo tamaño del buffer (ya se leyó el código de operación antes)
    if (recv(socket_cliente, &buffer_size, sizeof(int), MSG_WAITALL) != sizeof(int))
        exit(EXIT_FAILURE);

    void* buffer = malloc(buffer_size);
    if (buffer == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }

    // Recibo el buffer completo
    if (recv(socket_cliente, buffer, buffer_size, MSG_WAITALL) != buffer_size) {
        fprintf(stderr, "Error: recv tamaño esperado %d pero se recibió menos.\n", buffer_size);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    t_list* lista_parametros = list_create();

    int offset = 0;
    while (offset < buffer_size) {
        int param_size;
        memcpy(&param_size, buffer + offset, sizeof(int));
        offset += sizeof(int);

        void* param = malloc(param_size);
        if (param == NULL) {
            perror("Error al asignar memoria");
            exit(EXIT_FAILURE);
        }

        memcpy(param, buffer + offset, param_size);
        offset += param_size;

        list_add(lista_parametros, param);
    }

    free(buffer);
    return lista_parametros;
}

// WARNING: posible leak usando esta funcion (reserva memoria y no la libera), siempre liberar los mallocs despues de usar
t_list* recibir_paquete(int socket_cliente) {
    int buffer_size;

    if (recv(socket_cliente, &buffer_size, sizeof(int), MSG_WAITALL) != sizeof(int))
        exit(EXIT_FAILURE);

    void* buffer = malloc(buffer_size);
    if (buffer == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }

    if (recv(socket_cliente, buffer, buffer_size, MSG_WAITALL) != buffer_size) {
        free(buffer);
        exit(EXIT_FAILURE);
    }

    t_list* lista_parametros = list_create();

    int offset = 0;
    while (offset < buffer_size) {
        int param_size;
        memcpy(&param_size, buffer + offset, sizeof(int));
        offset += sizeof(int);

        void* param = malloc(param_size);
        if (param == NULL) {
            perror("Error al asignar memoria");
            exit(EXIT_FAILURE);
        }

        memcpy(param, buffer + offset, param_size);
        offset += param_size;

        list_add(lista_parametros, param);
    }

    free(buffer);
    return lista_parametros;
}

// WARNING: posible leak usando esta funcion (reserva memoria y no la libera), siempre liberar los mallocs despues de usar
t_list* recibir_2_enteros_sin_op(int socket) {
    int buffer_size;

    // Recibo tamaño del buffer (ya se leyó el código de operación antes)  
    if (recv(socket, &buffer_size, sizeof(int), MSG_WAITALL) != sizeof(int))
        exit(EXIT_FAILURE);

    void* buffer = malloc(buffer_size);
    if (buffer == NULL) {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }
    
    // Recibo el buffer completo
    if (recv(socket, buffer, buffer_size, MSG_WAITALL) != buffer_size) {
        free(buffer);
        exit(EXIT_FAILURE);
    }

    t_list* lista = list_create();
    int desp = 0;

    int entero1 = leer_entero(buffer, &desp);
    int entero2 = leer_entero(buffer, &desp);

    list_add(lista, (void *)(uintptr_t)entero1);
    list_add(lista, (void *)(uintptr_t)entero2);

    free(buffer);
    return lista;
}

bool recibir_enteros(int socket, int* destino, int cantidad) {
    int total_bytes = cantidad * sizeof(int);
    int recibidos = recv(socket, destino, total_bytes, MSG_WAITALL);
    return recibidos == total_bytes;
}