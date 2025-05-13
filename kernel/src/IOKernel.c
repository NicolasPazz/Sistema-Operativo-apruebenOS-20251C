#include "../headers/IOKernel.h"

// funcion para procesar la peticion de IO de CPU -> Kernel

void procesar_IO_from_CPU(char** nombre_IO, uint16_t* cant_tiempo, t_pcb* pcb_a_io){

    // Llamamos a la syscall
    IO(*nombre_IO, *cant_tiempo, pcb_a_io);
}

bool recv_IO_from_CPU(int fd, char** nombre_IO, uint8_t* cant_tiempo) {
    size_t size_payload;
    if (recv(fd, &size_payload, sizeof(size_t), 0) != sizeof(size_t))
        return false;

    void* stream = malloc(size_payload);
    if (recv(fd, stream, size_payload, 0) != size_payload) {
        free(stream);
        return false;
    }
    deserializar_un_char_y_un_int(stream, nombre_IO, cant_tiempo);

    free(stream);
    return true;
}
