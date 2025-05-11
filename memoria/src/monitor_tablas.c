#include "../headers/monitor_tablas.h"

extern t_log* memoria_log;
extern t_config_memoria* cfg;

extern pthread_mutex_t MUTEX_TS_PATOTAS;
extern pthread_mutex_t MUTEX_TS_TRIPULANTES;
extern pthread_mutex_t MUTEX_TP_PATOTAS;
extern pthread_mutex_t MUTEX_TID_PID_LOOKUP;

extern t_list* ts_patotas;
extern t_list* ts_tripulantes;

extern t_list* tp_patotas;
extern t_list* tid_pid_lookup;
//extern frame_swap_t* tabla_frames_swap;
extern uint32_t global_TUR;

static uint32_t static_pid;
static uint32_t static_tid;
static uint32_t static_inicio;
static uint32_t static_nro_pag;
static uint32_t static_nro_frame;

extern void* memoria_principal; // solo por un print
extern void* area_swap;         // solo por un print