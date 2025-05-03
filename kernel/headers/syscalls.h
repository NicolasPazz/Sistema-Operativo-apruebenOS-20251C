#ifndef SYSCALLS_H
#define SYSCALLS_H

/////////////////////////////// Includes ///////////////////////////////
#include "kernel.h"

/////////////////////////////// Prototipos ///////////////////////////////
/**
 * @brief Crea un proceso y lo deja en estado NEW
 * @param nombre_archivo: Nombre del archivo de pseudocodigo que ejecuta el proceso
 * @param tam_memoria: Tamanio del proceso en memoria
 * 
 * @return Retorna el PCB del proceso creado
 */
void INIT_PROC(char* nombre_archivo, uint16_t tam_memoria);


/**
 * @brief Hace un Dump en memoria del proceso que la llama
 * 
 * @details Bloquea el proceso que la use hasta que memoria confirme la finalizacion del Dump
 * en caso de Error el proceso pasa a EXIT, si no hay error pasa a READY
 */
void DUMP_MEMORY();


/**
 * @brief Hace uso de un dispositivo de IO
 * @param nombre_io: Nombre del IO
 * @param tiempo_a_usar: Tiempo en ms que se usara el IO
 * 
 * @details Hay que validar que la IO exista en el Sistema si no, se envia a EXIT el proceso que la solicito
 * Si existia el IO el proceso va a BLOCKED y se lo agrega a la cola de bloqueados por la IO solicitada
 * Si el IO esta libre se lo usa durante el tiempo_a_usar y se le envia el PID que lo esta usando
 */
void IO(char* nombre_io, uint16_t tiempo_a_usar);

#endif /* SYSCALLS_H */