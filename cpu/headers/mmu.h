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
int traducir_direccion(int direccion_logica, int* desplazamiento);
bool tlb_buscar(int pagina, int* frame);
void tlb_insertar(int pagina, int frame);
bool tlb_habilitada(void);
int timestamp_actual(void);
int seleccionar_victima_tlb(void);
int cargar_configuracion(char* path);


#endif // MMU_H
