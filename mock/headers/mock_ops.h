#ifndef MOCK_OPS_H
#define MOCK_OPS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/////////////////////////////// Prototipos ///////////////////////////////
// Funciones mockeadas para cada op_code
void MENSAJE_OP_mock(void);
void PAQUETE_OP_mock(void);
void IO_OP_mock(void);
bool INIT_PROC_OP_mock(int cliente_socket);
void DUMP_MEMORY_OP_mock(void);
void EXIT_OP_mock(void);
void EXEC_OP_mock(void);
void INTERRUPCION_OP_mock(void);
void PEDIR_INSTRUCCION_OP_mock(int cliente_socket);
void PEDIR_CONFIG_CPU_OP_mock(void);
void IO_FINALIZADA_OP_mock(void);
void FINALIZAR_PROC_OP_mock(void);
void DEBUGGER_mock(void);
void NOOP_OP_mock(void);
void WRITE_OP_mock(void);
void READ_OP_mock(void);
void GOTO_OP_mock(void);
void PEDIR_PAGINA_OP_mock(void);

#endif /* MOCK_OPS_H */