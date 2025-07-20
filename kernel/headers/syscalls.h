#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "kernel.h"
#include "types.h"

void INIT_PROC(char *nombre_archivo, int tam_memoria);
void DUMP_MEMORY(t_pcb *pcb_dump);
void bloquear_pcb_por_dump_memory(t_pcb *pcb);
void enviar_dump_memory_a_memoria(t_pcb *pcb);
void fin_dump_memory(int pid_finalizado, t_respuesta respuesta);
void IO(char *nombre_io, int tiempo_a_usar, t_pcb *pcb_a_io);
io *get_io(char *nombre_io);
void bloquear_pcb_por_io(char *nombre_io, t_pcb *pcb, int tiempo_a_usar);
void fin_io(io *dispositivo, int pid_finalizado);
void EXIT(t_pcb **ptr_pcb_a_finalizar);
t_pcb_io *buscar_y_remover_pcb_io_por_dispositivo_y_pid(io *dispositivo, int pid);
io *buscar_io_por_fd(int fd);
io *buscar_y_remover_io_por_fd(int fd);
io *buscar_io_por_nombre(char *nombre);
void actualizar_metricas_finalizacion(t_pcb *pcb);

#endif /* SYSCALLS_H */