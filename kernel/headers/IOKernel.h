#ifndef IOKERNEL_H_
#define IOKERNEL_H_

#include "syscalls.h"
#include "kernel.h"
#include "types.h"
#include <semaphore.h>

void procesar_IO_from_CPU(char** nombre_IO, uint16_t* cant_tiempo, t_pcb* pcb_a_io);
bool recv_IO_from_CPU(int fd, char** nombre_IO, uint8_t* cant_tiempo);

#endif /* IOKERNEL_H_ */