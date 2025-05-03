#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"

t_instruccion* fetch(int pc, int pid){
    // t_instruccion* instruccion ;//pedir_instruccion_memoria(pc);
    // if (instruccion == NULL)
    // {
    //     log_trace("No existe instruccion con el program counter: %d", pc);
    //     EXIT_FAILURE;
    // }
    // log_info(cpu_log, "PID: %i - FETCH - Program Counter: %i", pid, pc);
    // return instruccion;

    t_paquete* paquete = crear_paquete_op(PEDIR_INSTRUCCION_OP);
    agregar_entero_a_paquete(paquete, pc);
    agregar_entero_a_paquete(paquete, pid);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    //int codigo = recibir_operacion(fd_memoria);

    t_instruccion* instruccion = recibir_instruccion(fd_memoria);

    return instruccion;
}