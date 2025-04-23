#ifndef CACHE_H
#define CACHE_H
#include "../headers/cache.h"
#include "../headers/mmu.h"
#include "../headers/init.h"
#include "../../utils/headers/sockets.h"

typedef struct {
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


t_cache_paginas* inicializar_cache();
int buscar_pagina_en_cache (t_cache_paginas* cache, int numero_pagina);
int seleccionar_victima_clock(t_cache_paginas* cache);
int seleccionar_victima_clock_m (t_cache_paginas* cache);
char* acceder_a_pagina_en_cache(t_cache_paginas* cache, int numero_pagina);
void desalojar_proceso_cache(t_cache_paginas* cache);
void liberar_cache(t_cache_paginas* cache);
bool cache_habilitada(t_cache_paginas* cache); 

#endif