#include "../headers/mmu.h"
#include "../../utils/headers/sockets.h"

t_direccion_fisica transformar_a_fisica(int direccion_logica, int nro_pagina, int tamanio_pagina, int cantidad_entradas){
    t_direccion_fisica direccion_fisica;
    direccion_fisica.nro_pagina = floor(direccion_logica / tamanio_pagina);
    direccion_fisica.entrada_nivel_x = floor(nro_pagina /cantidad_entradas); // esto deberia ser un array creo yo??
    direccion_fisica.desplazamiento = direccion_logica % tamanio_pagina;
    return direccion_fisica;
}