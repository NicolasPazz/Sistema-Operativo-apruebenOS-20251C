#include "../headers/sockets.h"


/////////////////////////////// Log y Config ///////////////////////////////
t_log* iniciar_logger(char *file, char *process_name, bool is_active_console, t_log_level level) {
	t_log* nuevo_logger = log_create(file, process_name,is_active_console,level);
	if(nuevo_logger == NULL){
		perror("Error al crear el log");
		exit(EXIT_FAILURE);
	}
	return nuevo_logger;
}

t_config* iniciar_config(char* path) {
	t_config* nuevo_config = config_create(path);
	if(nuevo_config == NULL){
		perror("Error al internar cargar el config");
		exit(EXIT_FAILURE);
	}
	return nuevo_config;
}


/////////////////////////////// Conexiones ///////////////////////////////
int iniciar_servidor(char *puerto,t_log* logger, char* msj_server) {
	int socket_servidor;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	socket_servidor = socket(servinfo->ai_family,
                         servinfo->ai_socktype,
                         servinfo->ai_protocol);


	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);
	log_trace(logger,"SERVER: %s", msj_server);

	return socket_servidor;
}



int crear_conexion(char *ip, char* puerto) {
    struct addrinfo hints;
    struct addrinfo *server_info;
    int socket_cliente;
    int optval = 1;

    // Configurar hints para la búsqueda de direcciones
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Obtener la información del servidor
    if (getaddrinfo(ip, puerto, &hints, &server_info) != 0) {
        perror("Error en getaddrinfo");
        return -1;
    }

    // Crear el socket
    socket_cliente = socket(server_info->ai_family,
                           server_info->ai_socktype,
                           server_info->ai_protocol);
    if (socket_cliente < 0) {
        perror("Error al crear socket");
        freeaddrinfo(server_info);
        return -1;
    }

    // Configurar SO_REUSEADDR (Para evitar errores al ejecutar rápido el proyecto, sin antes haber liberado los puertos)
    if (setsockopt(socket_cliente, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("Error al configurar SO_REUSEADDR");
        close(socket_cliente);
        freeaddrinfo(server_info);
        return -1;
    }

    // Conectar al servidor
    if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        perror("Error al conectar");
        close(socket_cliente);
        freeaddrinfo(server_info);
        return -1;
    }

    freeaddrinfo(server_info);

    return socket_cliente;
}

int esperar_cliente(int socket_servidor, t_log* logger) {
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}

void liberar_conexion(int socket_cliente) {
	close(socket_cliente);
}

/////////////////////////////// Mensajes y paquetes ///////////////////////////////
cliente_data_t *crear_cliente_data(int fd, t_log* logger, char* cliente) {
    cliente_data_t *data = malloc(sizeof(cliente_data_t));
    if (data == NULL) {
        perror("Error al asignar memoria para cliente_data_t*");
        return NULL;
    }
    data->fd = fd;
    data->logger = logger;
    data->cliente = strdup(cliente);
    if (data->cliente == NULL) {
        perror("Error al asignar memoria para cliente en crear_cliente_data");
        free(data);
        return NULL;
    }
    return data;
}

int recibir_operacion(int socket_cliente) {
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void liberar_cliente_data(cliente_data_t *data) {
    if (data != NULL) {
        // Liberar la memoria de la cadena duplicada
        free(data->cliente);
        // Liberar la memoria de la estructura
        free(data);
    }
}

void* recibir_buffer(int* size, int socket_cliente) {
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void recibir_mensaje(int socket_cliente,t_log* logger) {
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	log_info(logger, "Me llego el mensaje: %s", buffer);
	free(buffer);
}

t_list* recibir_paquete(int socket_cliente) {
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

void enviar_mensaje(char* mensaje, int socket_cliente) {
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void crear_buffer(t_paquete* paquete) {
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(void) {
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio) {
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente) {
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete) {
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void paquete(int conexion) {
	
	char* leido;
	t_paquete* paquete = crear_paquete();


	leido = readline(">> ");
	
	while (strcmp(leido, "") != 0){
		agregar_a_paquete(paquete,leido,strlen(leido)+1);
		free(leido);
		leido = readline(">> ");
	}

	enviar_paquete(paquete,conexion);
	free(leido);
	eliminar_paquete(paquete);
}

void* serializar_paquete(t_paquete* paquete, int bytes) {
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}