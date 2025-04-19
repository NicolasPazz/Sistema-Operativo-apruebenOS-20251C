#ifndef MMU_H
#define MMU_H
#include "sockets.h"

typedef struct {
    uint32_t pagina;
    uint32_t frame;
    uint64_t tiempo_uso;
    uint32_t orden_fifo;
    bool valido;
} entrada_tlb_t;

typedef struct {
    uint32_t frame;
    char* contenido;
    bool bit_uso;
    bool bit_modificado;
} entrada_cache_t;

void inicializar_mmu();
uint32_t traducir_direccion(uint32_t direccion_logica, uint32_t* desplazamiento, char* datos);
bool tlb_buscar(uint32_t pagina, uint32_t* frame);
void tlb_insertar(uint32_t pagina, uint32_t frame);
bool tlb_habilitada();
uint64_t timestamp_actual();
int seleccionar_victima_tlb();


#endif // MMU_H
