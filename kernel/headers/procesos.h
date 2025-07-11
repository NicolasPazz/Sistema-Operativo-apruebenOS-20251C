#ifndef PROCESOS_H
#define PROCESOS_H

/////////////////////////////// Includes ///////////////////////////////

#include <stdint.h>
#include <stdio.h>
#include "kernel.h"
#include "types.h"

/////////////////////////////// Prototipos ///////////////////////////////

void mostrar_pcb(t_pcb *PCB);                           // Muestra por pantalla los valores de un PCB
void mostrar_metrica(const char *nombre, int *metrica); // Auxiliar para mostrar_pcb
void mostrar_colas_estados(void);
const char *estado_to_string(Estados estado); // Asocia el valor del enum con el nombre del estado (Para imprimir el nombre en vez de un numero)
void cambiar_estado_pcb(t_pcb *PCB, Estados nuevo_estado_enum);
bool transicion_valida(Estados actual, Estados destino); // Valida transiciones de estados en base al diagrama de 7 estados
t_list *obtener_cola_por_estado(Estados estado);
void bloquear_cola_por_estado(Estados estado);
void liberar_cola_por_estado(Estados estado);
void loguear_metricas_estado(t_pcb *pcb);
t_pcb *buscar_y_remover_pcb_por_pid(t_list *cola, int pid); // Busca y remueve un PCB por PID de una cola específica
t_pcb *buscar_pcb(int pid);

#endif /* PROCESOS_H */