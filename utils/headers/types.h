#ifndef UTILS_TYPES_H
#define UTILS_TYPES_H


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
	SOLICITAR_FRAME_PARA_ENTRADAS,

	// Ciclo de Instrucciones CPU
	PEDIR_INSTRUCCION_OP, // Fetch
	EXEC_OP, // Execute
	INTERRUPCION_OP, // Check Interrupt

	// Memoria
	PEDIR_CONFIG_CPU_OP,
	FINALIZAR_PROC_OP,

	// Testing
	DEBUGGER
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
} t_respuesta_memoria;


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

#endif /* UTILS_TYPES_H */