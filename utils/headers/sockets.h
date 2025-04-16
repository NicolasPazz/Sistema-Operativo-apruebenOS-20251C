#ifndef UTILS_SOCKETS_H_
#define UTILS_SOCKETS_H_
#define _POSIX_C_SOURCE 200809L
/////////////// C Libs ///////////////
#include <sys/types.h> 
#include <netdb.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <readline/readline.h>
//agrego libs
#include <stdint.h>
#include <semaphore.h>
#include <math.h>
/////////////// Commons ///////////////
#include<commons/log.h>
#include<commons/collections/list.h>
#include<commons/config.h>
//agrego commons
#include <commons/bitarray.h>
#include <commons/temporal.h>


/////////////////////////////// Estructuras compartidas ///////////////////////////////
typedef enum {
	MENSAJE,
	PAQUETE,
	NOOP,
	WRITE, 
	READ, 
	GOTO, 
	IO, 
	INIT_PROC, 
	DUMP_MEMORY, 
	EXIT,
	EXEC
} op_code;

typedef struct {
    int fd;
    t_log* logger;
    char* cliente;
} cliente_data_t;

typedef struct {
	int size;
	void* stream;
} t_buffer;

typedef struct {
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

//Momentaneo si dejarlo aca o solamente en modulo cpu
typedef struct{
    char* parametros1;
    char* parametros2;
    char* parametros3;
}t_instruccion;
// typedef struct {
//     int entradas[NIVELES_PAGINACION]; 
//     int desplazamiento;
// } t_direccion_logica; lo pusimos como char* no es un struct...

typedef struct {
    int nro_pagina;
	int entrada_nivel_x;
	int desplazamiento;
} t_direccion_fisica;

/////////////////////////////// Prototipos ///////////////////////////////
/////////////// Logs y Config///////////////
t_log* iniciar_logger(char *file, char *process_name, bool is_active_console, t_log_level level);
t_config* iniciar_config(char* path);

/////////////// Conexiones ///////////////
int iniciar_servidor(char *puerto,t_log* logger, char* msj_server);
int crear_conexion(char *ip, char* puerto);
int esperar_cliente(int socket_servidor, t_log* logger);
void atender_cliente(void* arg);
void liberar_conexion(int socket_cliente);

/////////////// Mensajes y paquetes ///////////////
cliente_data_t* crear_cliente_data(int fd, t_log* logger, char* cliente);
int recibir_operacion(int socket_cliente);
void liberar_cliente_data(cliente_data_t *data);
void* recibir_buffer(int* size, int socket_cliente);
void recibir_mensaje(int socket_cliente, t_log* logger);
t_list* recibir_paquete(int socket_cliente);
void enviar_mensaje(char* mensaje, int socket_cliente);
void crear_buffer(t_paquete* paquete);
t_paquete* crear_paquete(void);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void paquete(int conexion);
void* serializar_paquete(t_paquete* paquete, int bytes);
void iterator(char* value);

#endif
