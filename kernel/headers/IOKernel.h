#ifndef IOKERNEL_H_
#define IOKERNEL_H_

#include "syscalls.h"
#include "kernel.h"
#include "types.h"
#include <semaphore.h>

/**
 * @brief Procesa la solicitud de IO recibida de una CPU
 * 
 * @param nombre_IO Nombre del dispositivo IO solicitado
 * @param cant_tiempo Tiempo en ms que se usará el dispositivo
 * @param pcb_a_io PCB del proceso que solicita la IO
 */
void procesar_IO_from_CPU(char* nombre_IO, uint16_t cant_tiempo, t_pcb* pcb_a_io);

/**
 * @brief Recibe los datos de una solicitud IO desde una CPU
 * 
 * @param fd File descriptor de la conexión con la CPU
 * @param nombre_IO Puntero donde se almacenará el nombre del dispositivo IO
 * @param cant_tiempo Puntero donde se almacenará el tiempo de uso
 * @return true Si se recibieron los datos correctamente
 * @return false Si hubo un error al recibir los datos
 */
bool recv_IO_from_CPU(int fd, char** nombre_IO, uint16_t* cant_tiempo);

#endif /* IOKERNEL_H_ */