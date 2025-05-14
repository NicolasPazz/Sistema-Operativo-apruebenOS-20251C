#ifndef CICLO_DE_INSTRUCCION_H
#define CICLO_DE_INSTRUCCION_H
#include "sockets.h"

void ejecutar_ciclo_instruccion();
t_instruccion* recibir_instruccion(int conexion);
t_instruccion* fetch();
op_code decode(char* instruccion);
void execute(op_code tipo_instruccion, t_instruccion* instruccion);
void check_interrupt();
int seguir_ejecutando;
int pid_ejecutando;
int pid_interrupt;
int hay_interrupcion;
int pc;

#endif