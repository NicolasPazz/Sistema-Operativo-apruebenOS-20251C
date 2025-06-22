#ifndef IOKERNEL_H_
#define IOKERNEL_H_

#include "syscalls.h"
#include "kernel.h"
#include "types.h"
#include <semaphore.h>

/**
 * @brief Recibe los datos de una solicitud IO desde una CPU
 * 
 * @param fd File descriptor de la conexión con la CPU
 * @param nombre_IO Puntero donde se almacenará el nombre del dispositivo IO
 * @param cant_tiempo Puntero donde se almacenará el tiempo de uso
 * @return true Si se recibieron los datos correctamente
 * @return false Si hubo un error al recibir los datos
 */
bool recv_IO_from_CPU(int fd, char** nombre_IO, int* cant_tiempo, int* PC);

#endif /* IOKERNEL_H_ */