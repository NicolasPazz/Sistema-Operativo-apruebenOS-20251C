#ifndef PLANIFICADORES_H
#define PLANIFICADORES_H

/////////////////////////////// Includes ///////////////////////////////
#include "kernel.h"
#include "types.h"

/////////////////////////////// Prototipos ///////////////////////////////
void iniciar_planificador_corto_plazo(char* algoritmo);
t_pcb* planificar_por_fifo(void);
t_pcb* planificar_por_sjf(void);
t_pcb* planificar_por_srt(void);

#endif /* PLANIFICADORES_H */