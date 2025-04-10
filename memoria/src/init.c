#include "../headers/memoria.h"

/////////////////////////////// Inicialización de variables globales ///////////////////////////////
t_log* memoria_log;

int fd_memoria;
int fd_kernel;
int fd_cpu;

t_config* memoria_config;
char* PUERTO_ESCUCHA;
char* TAM_MEMORIA;
char* TAM_PAGINA;
char* ENTRADAS_POR_TABLA;
char* CANTIDAD_NIVELES;
char* RETARDO_MEMORIA;
char* PATH_SWAPFILE;
char* RETARDO_SWAP;
char* LOG_LEVEL;
char* DUMP_PATH;

void iniciar_config_memoria() {
    memoria_config = iniciar_config("memoria.config");

    PUERTO_ESCUCHA = config_get_string_value(memoria_config, "PUERTO_ESCUCHA");
    TAM_MEMORIA = config_get_string_value(memoria_config, "TAM_MEMORIA");
    TAM_PAGINA = config_get_string_value(memoria_config, "TAM_PAGINA");
    ENTRADAS_POR_TABLA = config_get_string_value(memoria_config, "ENTRADAS_POR_TABLA");
    CANTIDAD_NIVELES = config_get_string_value(memoria_config, "CANTIDAD_NIVELES");
    RETARDO_MEMORIA = config_get_string_value(memoria_config, "RETARDO_MEMORIA");
    PATH_SWAPFILE = config_get_string_value(memoria_config, "PATH_SWAPFILE");
    RETARDO_SWAP = config_get_string_value(memoria_config, "RETARDO_SWAP");
    LOG_LEVEL = config_get_string_value(memoria_config, "LOG_LEVEL");
    DUMP_PATH = config_get_string_value(memoria_config, "DUMP_PATH");

    if (PUERTO_ESCUCHA && TAM_MEMORIA && TAM_PAGINA &&
        ENTRADAS_POR_TABLA && CANTIDAD_NIVELES && RETARDO_MEMORIA &&
        PATH_SWAPFILE && RETARDO_SWAP && LOG_LEVEL && DUMP_PATH) {
    } else {
        printf("Error al leer memoria.config\n");
    }
}

void iniciar_logger_memoria() {
    memoria_log = iniciar_logger("memoria.log", "memoria", 1, log_level_from_string(LOG_LEVEL));
    if (memoria_log == NULL) {
        printf("Error al iniciar memoria logs\n");
    } else {
        log_info(memoria_log, "Memoria logs iniciados correctamente!");
    }
}

void iniciar_conexiones_memoria(){
    //////////////////////////// Iniciar Server Memoria ////////////////////////////
    fd_memoria = iniciar_servidor(PUERTO_ESCUCHA, memoria_log, "Server Memoria iniciado");

    //////////////////////////// Esperar al Cliente Kernel ////////////////////////////
    log_info(memoria_log,"Esperando la conexion del Kernel...");
    fd_kernel = esperar_cliente(fd_memoria, memoria_log);

    //////////////////////////// Esperar al Cliente CPU ////////////////////////////
    log_info(memoria_log,"Esperando la conexion del CPU ...");
    fd_cpu= esperar_cliente(fd_memoria,memoria_log);
}