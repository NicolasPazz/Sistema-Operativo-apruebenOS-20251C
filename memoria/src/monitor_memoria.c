#include "../headers/monitor_memoria.h"

extern t_log* logger; // por funciones de debug
extern t_config_memoria* cfg;
extern t_list* segmentos_libres;
extern t_list* segmentos_usados;

//extern frame_t* tabla_frames;

extern void* memoria_principal;

/// Funciones vacías para compatibilidad - los mutex/semáforos fueron removidos
/// ya que no se utilizan en el código actual

void iniciar_mutex() {
    // Función vacía - no hay mutex ni semáforos que inicializar
    log_debug(logger, "Monitor de memoria inicializado (sin sincronización por ahora)");
}

void finalizar_mutex() {
    // Función vacía - no hay mutex ni semáforos que liberar  
    log_debug(logger, "Monitor de memoria finalizado");
}