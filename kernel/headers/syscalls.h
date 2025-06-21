#ifndef SYSCALLS_H
#define SYSCALLS_H

/////////////////////////////// Includes ///////////////////////////////
#include "kernel.h"
#include "types.h"

/////////////////////////////// Prototipos ///////////////////////////////
/**
 * @brief Crea un proceso y lo deja en estado NEW
 * @param nombre_archivo: Nombre del archivo de pseudocodigo que ejecuta el proceso
 * @param tam_memoria: Tamanio del proceso en memoria
 * 
 */
void INIT_PROC(char* nombre_archivo, int tam_memoria);

/**
 * @brief Hace un Dump en memoria del proceso que la llama
 * 
 * @details Bloquea el proceso que la use hasta que memoria confirme la finalizacion del Dump
 * en caso de Error el proceso pasa a EXIT, si no hay error pasa a READY
 */
void DUMP_MEMORY(void);

/**
 * @brief Hace uso de un dispositivo de IO
 * @param nombre_io: Nombre del IO
 * @param tiempo_a_usar: Tiempo en ms que se usara el IO
 * @param pcb_a_io: PCB del proceso que solicita la IO
 * 
 * @details Hay que validar que la IO exista en el Sistema si no, se envia a EXIT el proceso que la solicito
 * Si existia el IO el proceso va a BLOCKED y se lo agrega a la cola de bloqueados por la IO solicitada
 * Si el IO esta libre se lo usa durante el tiempo_a_usar y se le envia el PID que lo esta usando
 */
void IO(char* nombre_io, int tiempo_a_usar, t_pcb* pcb_a_io);

io* get_io(char* nombre_io);

/**
 * @brief Bloquea un PCB por un dispositivo IO y lo agrega a la cola de bloqueados
 * @param nombre_io: nombre del dispositivo IO que bloquea al proceso
 * @param pcb: PCB del proceso a bloquear
 * @param tiempo_a_usar: Tiempo en ms que se usará el dispositivo
 */
void bloquear_pcb_por_io(char* nombre_io, t_pcb* pcb, int tiempo_a_usar);

/**
 * @brief Procesa la finalización de una operación IO
 * @param dispositivo: Dispositivo IO que terminó su operación
 * @param pid_finalizado: PID del proceso que finalizó la operación IO
 */
void fin_io(io* dispositivo, int pid_finalizado);

/**
 * @brief Finaliza un proceso y libera sus recursos
 * @param pcb_a_finalizar: PCB del proceso a finalizar
 */
void EXIT(t_pcb* pcb_a_finalizar);

#endif /* SYSCALLS_H */