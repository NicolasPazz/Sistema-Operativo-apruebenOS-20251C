#ifndef CICLO_DE_INSTRUCCION_H
#define CICLO_DE_INSTRUCCION_H
#include "sockets.h"




void ejecutar_ciclo_instruccion(int pc, int pid);
t_instruccion* fetch(int pc, int pid);
op_code decode(char* instruccion);
void execute(op_code tipo_instruccion, t_instruccion* instruccion);
void func_noop();
void func_write(char* direccion, char* datos);
void func_read(int direccion, int tamanio);
void func_goto(char* valor);
void func_io(char* tiemp);
void func_init_proc(t_instruccion* instruccion);
void func_dump_memory();
void func_exit();
void pedir_funcion_memoria();
void check_interrupt();
t_instruccion* recibir_instruccion(int conexion);


#endif