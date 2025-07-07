#ifndef CPUKERNEL_H_
#define CPUKERNEL_H_
#include "kernel.h"
#include "types.h"

/////////////////////////////// Prototipos ///////////////////////////////
/**
 * @brief Encuentra la CPU por su fd y devuelve el PID del proceso que está ejecutando
 * @param fd: File descriptor de la CPU
 * @param instruccion: Código de operación que está procesando
 * @return PID del proceso ejecutando en esa CPU
 */
int get_pid_from_cpu(int fd, op_code instruccion);

cpu* get_cpu_from_fd(int fd);

/**
 * @brief Busca y remueve una CPU por su file descriptor
 * @param fd: File descriptor de la CPU a remover
 * @return Puntero a la estructura CPU removida, NULL si no se encuentra
 */
cpu* buscar_y_remover_cpu_por_fd(int fd);

#endif /* CPUKERNEL_H_ */