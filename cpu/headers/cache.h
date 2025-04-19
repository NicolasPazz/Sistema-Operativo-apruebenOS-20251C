#ifndef CACHE_H
#define CACHE_H
#include "sockets.h"

typedef struct {
    uint32_t frame;
    char* contenido;
    bool bit_uso;
    bool bit_modificado;
} entrada_cache_t;

void cache_habilitada();
void cache_contiene_pagina(uint32_t frame);
void cache_escribir(uint32_t frame, uint32_t desplazamiento, char* datos);

#endif