
#ifndef CACHE_H
#define CACHE_H
#include "../headers/cache.h"
#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../../utils/headers/sockets.h"

typedef struct {
    int pid;
    int numero_pagina;
    char* contenido;
    bool modificado;
    int bit_referencia;
} t_entrada_cache;


typedef struct {
    int cantidad_entradas;
    t_entrada_cache* entradas;
    char* algoritmo_reemplazo;
    int puntero_clock;
} t_cache_paginas;

extern t_cache_paginas* cache;

t_cache_paginas* inicializar_cache(void);
int buscar_pagina_en_cache (int pid, int numero_pagina);
int seleccionar_victima_clock();
int seleccionar_victima_clock_m ();
char* acceder_a_pagina_en_cache(int pid, int numero_pagina);
void desalojar_proceso_cache();
void liberar_cache();
bool cache_habilitada();
void cache_modificar(int pid, int frame, char* datos);
void cache_escribir(int pid, int frame, char* datos, bool modificado);
char* cache_leer(int pid, int numero_pagina);
void enviar_actualizar_pagina_completa(int pid, int direccion_fisica, void* contenido);
#endif