#ifndef PLANIFICADORES_H
#define PLANIFICADORES_H

/////////////////////////////// Includes ///////////////////////////////


/////////////////////////////// Prototipos ///////////////////////////////
/**
 * @brief Crea un proceso y lo dejar en estado NEW
 * @param nombre_archivo: Nombre del archivo de pseudocódigo que ejecuta el proceso
 * @param tam_memoria: Tamaño del proceso en memoria
 * 
 * @return Retorna el PCB del proceso creado
 */
t_pcb INIT_PROC(char* nombre_archivo, uint8_t tam_memoria);

#endif /* PLANIFICADORES_H */