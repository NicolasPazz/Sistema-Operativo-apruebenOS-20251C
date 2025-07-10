#ifndef UTILS_TYPES_H
#define UTILS_TYPES_H
#include "colores.h"

/////////////////////////////// Estructuras compartidas ///////////////////////////////

// Codigos de operaciones entre modulos
typedef enum {
	// Mensajes
	MENSAJE_OP,
	PAQUETE_OP,

	// Syscalls Procesos
	INIT_PROC_OP, 
	EXIT_OP,
	// Syscalls Memoria
	DUMP_MEMORY_OP, 
	// Syscalls IO
	IO_OP, 

	// Interrupciones a Kernel
	IO_FINALIZADA_OP,

	// Instrucciones CPU
	NOOP_OP,
	WRITE_OP, 
	READ_OP, 
	GOTO_OP,
	PEDIR_PAGINA_OP,

	// Ciclo de Instrucciones CPU
	PEDIR_INSTRUCCION_OP, // Fetch
	EXEC_OP, // Execute
	INTERRUPCION_OP, // Check Interrupt

	// Memoria
	PEDIR_CONFIG_CPU_OP,
	INSTRUCCION_A_CPU_OP,
	FINALIZAR_PROC_OP,

	// Testing
	DEBUGGER,

	// Operaciones adicionales de memoria
	ACCESO_TABLA_PAGINAS_OP,      // Acceso a tabla de páginas - devuelve el Frame de un PID y su Pagina
	ACCESO_ESPACIO_USUARIO_OP,    // Acceso a espacio de usuario - lectura/escritura
	LEER_PAGINA_COMPLETA_OP,      // Leer página completa desde dirección física
	ACTUALIZAR_PAGINA_COMPLETA_OP, // Actualizar página completa en dirección física
	CHECK_MEMORY_SPACE_OP,        // Consultar si hay espacio suficiente en memoria
	
	// Operaciones de suspensión/des-suspensión
	SUSPENDER_PROCESO_OP,         // Suspender proceso (mover páginas a SWAP)
	DESUSPENDER_PROCESO_OP,       // Des-suspender proceso (cargar páginas desde SWAP)
	
	// Operación de shutdown
	SHUTDOWN_OP                   // Notificar shutdown a módulos
} op_code;

// Handshake
typedef enum {
    HANDSHAKE_MEMORIA_CPU,
    HANDSHAKE_MEMORIA_KERNEL,
    HANDSHAKE_CPU_KERNEL_INTERRUPT,
    HANDSHAKE_CPU_KERNEL_DISPATCH,
    HANDSHAKE_IO_KERNEL
} handshake_code;

// Estructuras de serialización
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

/*
 typedef struct {
     int entradas[NIVELES_PAGINACION]; 
     int desplazamiento;
} t_direccion_logica; lo pusimos como char* no es un struct...
*/

typedef struct {
    int nro_pagina;
	int entrada_nivel_x;
	int desplazamiento;
} t_direccion_fisica;

// Respuestas de Memoria
typedef enum {
    OK,
	ERROR
} t_respuesta;

// Instrucciones de CPU
typedef struct{
    char* parametros1;
    char* parametros2;
    char* parametros3;
} t_instruccion;

// Extención de t_instruccion para incluir el tipo de operación
typedef struct {
    t_instruccion instruccion_base;
    op_code tipo;                   // Tipo de operación (NOOP_OP, WRITE_OP, etc.)
} t_extended_instruccion;

// IOs
typedef struct {
    int pid;
    long tiempo_io;
} t_pedido_io;

// Estructuras adicionales para los 4 tipos de acceso específicos de memoria
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

#endif /* UTILS_TYPES_H */