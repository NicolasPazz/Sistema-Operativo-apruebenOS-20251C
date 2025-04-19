#ifndef PLANIFICADORES_H
#define PLANIFICADORES_H

/////////////////////////////// Includes ///////////////////////////////
#include "kernel.h"
#include "types.h"

/////////////////////////////// Prototipos ///////////////////////////////
void iniciar_planificador_corto_plazo(char* algoritmo);
void dispatch(t_pcb* proceso_a_ejecutar);
t_pcb* elegir_por_fifo(void);
void* menor_rafaga(void* a, void* b);
t_pcb* elegir_por_sjf(void);
t_pcb* elegir_por_srt(void);

#endif /* PLANIFICADORES_H */