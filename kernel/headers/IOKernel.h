#ifndef IOKERNEL_H_
#define IOKERNEL_H_

#include "syscalls.h"
#include "kernel.h"
#include "types.h"

io *get_io(char *nombre_io);
io *buscar_io_por_fd(int fd);
io *buscar_io_por_nombre(char *nombre);
bool esta_libre_io(io *dispositivo);
io *io_disponible(char *nombre);
void bloquear_pcb_por_io(char *nombre_io, t_pcb *pcb, int tiempo_a_usar);
void enviar_io(io *dispositivo, t_pcb *pcb, int tiempo_a_usar);
void verificar_procesos_bloqueados(io *io);
t_pcb_io *obtener_pcb_esperando_io(char *nombre_io);
void exit_procesos_relacionados(io *io);

#endif /* IOKERNEL_H_ */