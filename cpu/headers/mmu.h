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

void inicializar_mmu(void);
uint32_t traducir_direccion(uint32_t direccion_logica, uint32_t* desplazamiento, char* datos);
bool tlb_buscar(uint32_t pagina, uint32_t* frame);
void tlb_insertar(uint32_t pagina, uint32_t frame);
bool tlb_habilitada(void);
uint64_t timestamp_actual(void);
int seleccionar_victima_tlb(void);


#endif // MMU_H
