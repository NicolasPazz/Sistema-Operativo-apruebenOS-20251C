#ifndef TYPES_H
#define TYPES_H

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
    tipo_conexion_cpu tipo_conexion;
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

#endif /* TYPES_H */