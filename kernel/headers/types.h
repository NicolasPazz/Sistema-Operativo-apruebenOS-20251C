#ifndef TYPES_H
#define TYPES_H

#include "../../utils/headers/sockets.h"

/////////////////////////////// TADs ///////////////////////////////

    ///// PCB
typedef struct PCB {
    int PID;
    int PC;
    int ME[7];
    int MT[7];
    int Estado;
    double tiempo_inicio_exec;
    double estimacion_rafaga;
    char* path;
    int tamanio_memoria;
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

typedef struct CPU {
    int fd;
    int id;
    int pid;
    tipo_conexion_cpu tipo_conexion;
    op_code instruccion_actual;
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
    int tiempo_a_usar;  // Tiempo en ms que se va a usar el dispositivo IO
} t_pcb_io;

#endif /* TYPES_H */