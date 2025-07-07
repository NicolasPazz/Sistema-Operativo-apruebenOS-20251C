#ifndef COMUNICACION_H_
#define COMUNICACION_H_

#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <commons/log.h>
#include <commons/config.h>
#include <../../utils/headers/sockets.h>
#include <../../utils/headers/utils.h>

#include "init_memoria.h"
#include "interfaz_memoria.h"
#include "manejo_memoria.h"

/////////////////////////////// Declaracion de variables globales ///////////////////////////////
extern t_log* logger;
extern t_config* memoria_config;

extern int fd_memoria;
extern int fd_kernel;
extern int fd_cpu;

extern char* PUERTO_ESCUCHA;
extern char* TAM_MEMORIA;
extern char* TAM_PAGINA;
extern char* ENTRADAS_POR_TABLA;
extern char* CANTIDAD_NIVELES;
extern char* RETARDO_MEMORIA;
extern char* PATH_SWAPFILE;
extern char* RETARDO_SWAP;
extern char* LOG_LEVEL;
extern char* DUMP_PATH;

/////////////////////////////// Prototipos ///////////////////////////////
void iniciar_logger_memoria(void);
void procesar_conexion(void*);
void procesar_cod_ops(op_code cop, int cliente_socket);

int iniciar_conexiones_memoria(char* puerto, t_log* logger);
int server_escuchar(char* server_name, int server_socket);

// Funciones para manejar operaciones específicas
void procesar_write_op(int cliente_socket);
void procesar_read_op(int cliente_socket);

#endif 
