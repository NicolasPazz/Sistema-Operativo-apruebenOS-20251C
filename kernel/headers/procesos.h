#ifndef PROCESOS_H
#define PROCESOS_H

#include <stdint.h>
#include <stdio.h>
#include "kernel.h"
#include "types.h"

void mostrar_pcb(t_pcb *PCB);
void mostrar_metrica(const char *nombre, int *metrica);
void mostrar_colas_estados();
const char *estado_to_string(Estados estado);
void cambiar_estado_pcb(t_pcb *PCB, Estados nuevo_estado_enum);
void cambiar_estado_pcb_srt(t_pcb *PCB, Estados nuevo_estado_enum);
void cambiar_estado_pcb_mutex(t_pcb *PCB, Estados nuevo_estado_enum);
void cambiar_estado_pcb_mutex_srt(t_pcb *PCB, Estados nuevo_estado_enum);
bool transicion_valida(Estados actual, Estados destino);
t_list *obtener_cola_por_estado(Estados estado);
void bloquear_cola_por_estado(Estados estado);
void liberar_cola_por_estado(Estados estado);
void loguear_metricas_estado(t_pcb *pcb);
t_pcb *buscar_y_remover_pcb_por_pid(t_list *cola, int pid);
t_pcb *buscar_pcb(int pid);
t_pcb *ultimo_proceso_new();
t_pcb *ultimo_si_es_menor();
void verificar_procesos_restantes();
void liberar_pcb(t_pcb *pcb);

#endif /* PROCESOS_H */