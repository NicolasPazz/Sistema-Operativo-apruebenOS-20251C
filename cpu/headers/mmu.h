
#ifndef MMU_H
#define MMU_H
#include "sockets.h"
#include "../../memoria/headers/init_memoria.h"
typedef struct {
    int pid;            // PID al que pertenece la entrada
    int pagina;
    int frame;
    int tiempo_uso;     // Para LRU
    int orden_fifo;     // Para FIFO
    bool valido;
} entrada_tlb_t;

extern t_list* tlb;
extern int orden_fifo;
extern t_config_memoria* cfg_memoria;

void inicializar_mmu(void);
int traducir_direccion_fisica(int direccion_logica);
bool tlb_buscar(int pid, int pagina, int* frame);
void tlb_insertar(int pid, int pagina, int frame);
bool tlb_habilitada(void);
long timestamp_actual(void);
int seleccionar_victima_tlb(void);
int cargar_configuracion(char* path);
void desalojar_proceso_tlb(int pid);
void desalojar_proceso_cache(int pid);

#endif // MMU_H
