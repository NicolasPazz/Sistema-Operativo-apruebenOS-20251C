#include "../headers/monitor_tablas.h"

extern t_log* logger;
extern t_config_memoria* cfg;

extern t_list* tid_pid_lookup;
//extern frame_swap_t* tabla_frames_swap;
extern int global_TUR;

static int static_pid;
static int static_tid;
static int static_inicio;
static int static_nro_pag;
static int static_nro_frame;

extern void* memoria_principal; // solo por un print
extern void* area_swap;         // solo por un print