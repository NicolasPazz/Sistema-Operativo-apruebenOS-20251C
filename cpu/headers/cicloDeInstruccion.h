#ifndef CICLO_DE_INSTRUCCION_H
#define CICLO_DE_INSTRUCCION_H
#include "sockets.h"



void ejecutar_ciclo_instruccion(int pc, int pid);
t_instruccion* fetch(int pc, int pid);
op_code decode(t_instruccion * instruccion);
void execute(op_code tipo_instruccion, t_instruccion* instruccion);
void func_noop(t_instruccion* instruccion);
void func_write(t_instruccion* instruccion);
void func_read(t_instruccion* instruccion);
void func_goto(t_instruccion* instruccion);
void func_io(t_instruccion* instruccion);
void func_init_proc(t_instruccion* instruccion);
void func_dump_memory(t_instruccion* instruccion);
void func_exit(t_instruccion* instruccion);
void pedir_funcion_memoria();


#endif