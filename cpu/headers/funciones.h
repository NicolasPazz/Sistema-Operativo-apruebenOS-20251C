#ifndef FUNCIONES_H
#define FUNCIONES_H
#include "sockets.h"
void func_noop();
void func_write(char* direccion, char* datos);
void func_read(int direccion, int tamanio);
void func_goto(char* valor);
void func_io(char* tiemp);
void func_init_proc(t_instruccion* instruccion);
void func_dump_memory();
void func_exit();
void pedir_funcion_memoria();
#endif