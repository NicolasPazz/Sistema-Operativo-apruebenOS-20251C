#ifndef MMU_H
#define MMU_H
#include "sockets.h"
#include "../../memoria/headers/init_memoria.h"
typedef struct {
    int pagina; //pagina y frame no eran lo mismo?
    int frame;
    int tiempo_uso;
    int orden_fifo;
    bool valido;
} entrada_tlb_t;

//extern t_cache_paginas* cache;
extern t_list* tlb;
extern int orden_fifo;
extern t_config_memoria* cfg_memoria;

void inicializar_mmu(void);
int traducir_direccion_fisica(int direccion_logica);
bool tlb_buscar(int pagina, int* frame);
void tlb_insertar(int pagina, int frame);
bool tlb_habilitada(void);
long timestamp_actual(void);
int seleccionar_victima_tlb(void);
int cargar_configuracion(char* path);
void desalojar_proceso_tlb();
void desalojar_proceso_cache();

#endif // MMU_H
