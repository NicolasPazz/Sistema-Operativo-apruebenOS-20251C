#ifndef TYPES_H
#define TYPES_H

#include "../../utils/headers/sockets.h"

/////////////////////////////// TADs ///////////////////////////////

    ///// PCB
typedef struct PCB {
    uint16_t PID;
    uint16_t PC;
    uint16_t ME[7];
    uint16_t MT[7];
    uint16_t Estado;
    double tiempo_inicio_exec;
    double estimacion_rafaga;
    char* path;
    uint16_t tamanio_memoria;
} t_pcb;

typedef enum Estados {
    NEW,
    READY,
    EXEC,
    BLOCKED,
    SUSP_READY,
    SUSP_BLOCKED,
    EXIT_ESTADO
} Estados;

typedef struct {
    t_pcb* pcb;
    t_temporal* cronometro;
} t_pcb_temporal;

    ///// CPU
typedef enum {
    CPU_DISPATCH,
    CPU_INTERRUPT
} tipo_conexion_cpu;

typedef struct {
    int fd;
    int id;
    int pid;
    tipo_conexion_cpu tipo_conexion;
    op_code instruccion_actual;
    // syscall -> la proceso -> termino: borro
    // pid = get_pid_from_cpu(syscall IO) -> antes del return del pid -> cpu -> syscall = null;
    // cpu -> instruccion -> kernel where instruccion : IO
} cpu;

    ///// IO
typedef enum {
    IO_DISPONIBLE,
    IO_OCUPADO
} estado_io;

typedef struct {
    int fd;
    char* nombre;
    estado_io estado;
} io;

    ///// PCBs IO
typedef struct PCB_IO{
    t_pcb* pcb;
    io* io;
    uint16_t tiempo_a_usar;  // Tiempo en ms que se va a usar el dispositivo IO
} t_pcb_io;

#endif /* TYPES_H */