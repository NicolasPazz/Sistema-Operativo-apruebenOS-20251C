#include "../headers/cicloDeInstruccion.h"
#include "../headers/init.h"
#include "../headers/mmu.h"
#include "../headers/instrucciones/execute.h"
#include "../headers/instrucciones/fetch.h"
#include "../headers/instrucciones/decode.h"
#include "../headers/instrucciones/checkinterrupt.h"
#include "../headers/funciones/funciones.h"

int seguir_ejecutando;
int pid_ejecutando, pid_interrupt, hay_interrupcion, pc; // ver de donde sacar estos, ademas de donde setear el hay interrupcion
void ejecutar_ciclo_instruccion(int pc, int pid) {
    seguir_ejecutando = 1;
    while(seguir_ejecutando == 1){
        t_instruccion* instruccion = fetch(pc, pid);
        op_code tipo_instruccion = decode(instruccion->parametros1);
        //una idea despues ver de donde sacar el pc, si pasarlo por parametro o hacerlo global
        if(tipo_instruccion != GOTO_OP){
            pc++;
        }    
        execute(tipo_instruccion, instruccion);
        if(seguir_ejecutando){
            check_interrupt();
        }
    }
}