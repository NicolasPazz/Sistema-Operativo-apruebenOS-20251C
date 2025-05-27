#ifndef INIT_MEMORIA_H
#define INIT_MEMORIA_H


#include <stdint.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "monitor_memoria.h"
#include "monitor_tablas.h"
#include "manejo_memoria.h"
#include "estructuras.h"
#include "../../utils/headers/sockets.h"
#include "../../utils/headers/utils.h"

typedef struct {
    int PUERTO_ESCUCHA;
    int TAM_MEMORIA;
    int TAM_PAGINA;
    int ENTRADAS_POR_TABLA;
    int CANTIDAD_NIVELES;
    int RETARDO_MEMORIA;
    char* PATH_SWAPFILE;
    int RETARDO_SWAP;
    char* LOG_LEVEL;
    char* DUMP_PATH;
    //bool FIFO; !ojo x ahi nos sirve para identificar el alg de reemplazo
} t_config_memoria;


#define MODULENAME "MEMORIA"

uint8_t init(void);                 // inicializa loger, cfg, y semaforos
uint8_t cargar_configuracion(char* path); // carga cfg en strut cfg
//uint8_t cargar_memoria(void);       // Init de segmentos_libres -- por ahora no necesito nada similar

void iniciar_logger_memoria(void);
void cerrar_programa(void);
int server_escuchar(char*, int);

#endif