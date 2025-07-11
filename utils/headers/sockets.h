#ifndef UTILS_SOCKETS_H_
#define UTILS_SOCKETS_H_
#define MAX_STRING_SIZE 256

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
#include <stdbool.h>

/////////////// Commons ///////////////

#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/bitarray.h>
#include <commons/temporal.h>
#include <commons/string.h>
#include <commons/collections/dictionary.h>

#include "serializacion.h"
#include "types.h"

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
bool enviar_enteros(int socket, int* enteros, int cantidad);
void crear_buffer(t_paquete* paquete);
void paquete(int conexion);
t_paquete* crear_paquete(void);
t_paquete* crear_paquete_op(op_code codop);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void agregar_entero_a_paquete(t_paquete *paquete, int numero);
void agregar_entero_con_tamanio_a_paquete(t_paquete *paquete, int numero);
void agregar_string_a_paquete(t_paquete* paquete, char* cadena);
void* serializar_paquete(t_paquete* paquete, int bytes);
void eliminar_paquete(t_paquete* paquete);
bool enviar_operacion(int socket, op_code operacion);
void iterator(char* value);
char* leer_string(char* buffer, int* desplazamiento);
int leer_entero(char *buffer, int * desplazamiento);
t_list* recibir_2_enteros_sin_op(int socket);
bool recibir_enteros(int socket, int* destino, int cantidad);

#endif /* UTILS_SOCKETS_H_ */
