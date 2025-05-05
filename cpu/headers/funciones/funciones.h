#ifndef FUNCIONES_H
#define FUNCIONES_H
#include "sockets.h"
void func_noop(void);
void func_write(char* direccion, char* datos);
void func_read(int direccion, int tamanio);
void func_goto(char* valor);
void func_io(char* tiemp);
void func_init_proc(t_instruccion* instruccion);
void func_dump_memory(void);
void func_exit(void);
void pedir_funcion_memoria(void);
#endif