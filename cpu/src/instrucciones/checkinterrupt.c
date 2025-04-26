#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
void check_interrupt(int seguir_ejecutando) {
    hay_interrupcion = 0;
     if (pid_ejecutando == pid_interrupt) {
      seguir_ejecutando = 0;
        t_paquete* paquete_kernel = crear_paquete_op(INTERRUPCION_OP);
        agregar_entero_a_paquete(paquete_kernel, pid_ejecutando);
        agregar_entero_a_paquete(paquete_kernel, pc);
        enviar_paquete(paquete_kernel, fd_kernel_interrupt);
        eliminar_paquete(paquete_kernel);
 }
}