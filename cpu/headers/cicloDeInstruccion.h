#ifndef CICLO_DE_INSTRUCCION_H
#define CICLO_DE_INSTRUCCION_H
#include "sockets.h"
void ejecutar_ciclo_instruccion();
t_instruccion* recibir_instruccion_desde_memoria();
t_instruccion* fetch();
op_code decode(char* instruccion);
void execute(op_code tipo_instruccion, t_instruccion* instruccion);
void check_interrupt();
void liberar_instruccion(t_instruccion* instruccion);
extern int seguir_ejecutando;
extern int pid_ejecutando;
extern int pid_interrupt;
extern int hay_interrupcion;
extern int pc;
#endif