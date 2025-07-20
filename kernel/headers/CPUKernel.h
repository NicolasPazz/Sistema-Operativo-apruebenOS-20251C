#ifndef CPUKERNEL_H_
#define CPUKERNEL_H_

#include "kernel.h"
#include "types.h"

int get_pid_from_cpu(int fd, op_code instruccion);
cpu *get_cpu_from_fd(int fd);
cpu *buscar_y_remover_cpu_por_fd(int fd);
void liberar_cpu(cpu *cpu_a_eliminar);
cpu *proxima_cpu_libre();
void ejecutar_proceso(cpu *cpu_disponible, t_pcb *proceso_a_ejecutar);
cpu *hay_cpu_rafaga_restante_mayor();
cpu *get_cpu_dispatch_by_pid(int pid);
void interrumpir_ejecucion(cpu *cpu_a_desalojar);
int get_exec_pid_from_id(int id);

#endif /* CPUKERNEL_H_ */