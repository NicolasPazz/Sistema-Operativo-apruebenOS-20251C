#ifndef CICLO_DE_INSTRUCCION_H
#define CICLO_DE_INSTRUCCION_H
#include "sockets.h"

void ejecutar_ciclo_instruccion(int pc, int pid);
t_instruccion* recibir_instruccion(int conexion);
extern int seguir_ejecutando;
extern int pid_ejecutando;
extern int pid_interrupt;
extern int hay_interrupcion;
extern int pc;

#endif