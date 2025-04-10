#ifndef MEMORIA_H
#define MEMORIA_H

/////////////////////////////// Includes ///////////////////////////////
#include "../../utils/headers/sockets.h"

/////////////////////////////// Declaración de variables globales ///////////////////////////////
extern t_log* memoria_log;
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
void iniciar_config_memoria(void);
void iniciar_logger_memoria(void);
void iniciar_conexiones_memoria(void);

#endif /* MEMORIA_H */
