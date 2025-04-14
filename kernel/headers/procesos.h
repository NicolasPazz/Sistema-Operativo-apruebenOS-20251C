#ifndef PROCESOS_H
#define PROCESOS_H

/////////////////////////////// Includes ///////////////////////////////


/////////////////////////////// Estructuras ///////////////////////////////
typedef struct PCB {
    uint8_t PID;
    uint8_t PC;
    uint8_t ME[7];
    uint8_t MT[7];
} t_pcb;

typedef enum Estados {
    NEW,
    READY,
    EXEC,
    BLOCKED,
    SUSP_READY,
    SUSP_BLOCKED,
    EXIT
} Estados;


/////////////////////////////// Prototipos ///////////////////////////////

#endif /* PROCESOS_H */