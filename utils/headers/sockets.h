#ifndef UTILS_SOCKETS_H_
#define UTILS_SOCKETS_H_

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
#include <stdint.h>
#include <semaphore.h>
#include <math.h>
#include <errno.h>

/////////////// Commons ///////////////
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/bitarray.h>
#include <commons/temporal.h>
#include <commons/string.h>

#include "serializacion.h"

/////////////////////////////// Estructuras compartidas ///////////////////////////////
typedef enum {
	MENSAJE_OP,
	PAQUETE_OP,
	IO_OP, 
	INIT_PROC_OP, 
	DUMP_MEMORY_OP, 
	EXIT_OP,
	EXEC_OP,
	INTERRUPCION_OP,
	PEDIR_INSTRUCCION_OP,
	PEDIR_CONFIG_CPU_OP,
	IO_FINALIZADA_OP,
	FINALIZAR_PROC_OP,
  	DEBUGGER, // para probar
	SEND_PSEUDOCOD_FILE, // cod_op para mandar de kernel a memoria la ruta del archivo de pseudocodigo
	// Instrucciones de cpu
	NOOP_OP,
	WRITE_OP, 
	READ_OP, 
	GOTO_OP,
	PEDIR_PAGINA_OP,
	SOLICITAR_FRAME_PARA_ENTRADAS,
	// Códigos de operación para los 4 tipos de acceso específicos de la consigna
	ACCESO_TABLA_PAGINAS_OP,      // Acceso a tabla de páginas - devuelve número de marco
	ACCESO_ESPACIO_USUARIO_OP,    // Acceso a espacio de usuario - lectura/escritura
	LEER_PAGINA_COMPLETA_OP,      // Leer página completa desde dirección física
	ACTUALIZAR_PAGINA_COMPLETA_OP // Actualizar página completa en dirección física
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

typedef enum {
    OK,
	ERROR
} t_respuesta_memoria;

typedef enum {
  HANDSHAKE_MEMORIA_CPU,
  HANDSHAKE_MEMORIA_KERNEL,
  HANDSHAKE_CPU_KERNEL_INTERRUPT,
  HANDSHAKE_CPU_KERNEL_DISPATCH,
  HANDSHAKE_IO_KERNEL
} handshake_code;

// Estructura extendida de t_instruccion para incluir el tipo de operación
typedef struct {
    t_instruccion instruccion_base;  // Mantiene compatibilidad con la estructura existente
    op_code tipo;                   // Almacena el tipo de operación (NOOP_OP, WRITE_OP, etc.)
} t_extended_instruccion;
// Dentro de utils_sockets.h o io.h
typedef struct {
    int pid;
    long tiempo_io;
} t_pedido_io;

// Estructuras para los 4 tipos de acceso específicos de la consigna
typedef struct {
    int pid;
    int numero_pagina;
} t_acceso_tabla_paginas;

typedef struct {
    int pid;
    int direccion_fisica;
    int tamanio;
    bool es_escritura;  // true para escritura, false para lectura
    void* datos;        // Solo para escritura
} t_acceso_espacio_usuario;

typedef struct {
    int pid;
    int direccion_fisica;  // Debe coincidir con byte 0 de la página
} t_leer_pagina_completa;

typedef struct {
    int pid;
    int direccion_fisica;  // Debe coincidir con byte 0 de la página
    void* contenido_pagina; // Contenido completo de la página
} t_actualizar_pagina_completa;

/////////////////////////////// Prototipos ///////////////////////////////

/////////////// Logs y Config
t_log* iniciar_logger(char *file, char *process_name, bool is_active_console, t_log_level level);
t_config* iniciar_config(char* path);

/////////////// Conexiones 
int iniciar_servidor(char *puerto,t_log* logger, char* msj_server);
int crear_conexion(char *ip, char* puerto, t_log* logger);
int esperar_cliente(int socket_servidor, t_log* logger);
void atender_cliente(void* arg);
void liberar_conexion(int socket_cliente);
bool validar_handshake(int fd, handshake_code esperado, t_log* log);

/////////////// Mensajes y paquetes 
cliente_data_t* crear_cliente_data(int fd, t_log* logger, char* cliente);
void liberar_cliente_data(cliente_data_t *data);

op_code recibir_operacion(int socket_cliente);
t_instruccion* recibir_instruccion(int conexion);

void* recibir_buffer(int* size, int socket_cliente);
void recibir_mensaje(int socket_cliente, t_log* logger);
t_list* recibir_paquete(int socket_cliente);
t_list* recibir_contenido_paquete(int socket_cliente);
void enviar_mensaje(char* mensaje, int socket_cliente);
void enviar_paquete(t_paquete* paquete, int socket_cliente);

void crear_buffer(t_paquete* paquete);

void paquete(int conexion);
t_paquete* crear_paquete(void);
t_paquete* crear_paquete_op(op_code codop);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void agregar_entero_a_paquete(t_paquete *paquete, int numero);
void* serializar_paquete(t_paquete* paquete, int bytes);
void eliminar_paquete(t_paquete* paquete);

void iterator(char* value);
char* leer_string(char* buffer, int* desplazamiento);
int leer_entero(char *buffer, int * desplazamiento);
t_list* recibir_2_enteros_sin_op(int socket);

#endif /* UTILS_SOCKETS_H_ */
