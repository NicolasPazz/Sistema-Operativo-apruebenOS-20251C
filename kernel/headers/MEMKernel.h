#ifndef KERNEL_HEADERS_MEMKERNEL_H
#define KERNEL_HEADERS_MEMKERNEL_H

#include "types.h"
#include "../../utils/headers/sockets.h"

int conectar_memoria(); // abre conexión efímera + handshake
void desconectar_memoria(int fd);
bool inicializar_proceso_en_memoria(t_pcb *pcb);
bool hay_espacio_suficiente_memoria(int tamanio);
bool suspender_proceso(t_pcb *pcb);
bool desuspender_proceso(t_pcb *pcb);
bool finalizar_proceso_en_memoria(int pid);
bool dump_memory(int pid);

#endif /* KERNEL_HEADERS_MEMKERNEL_H */