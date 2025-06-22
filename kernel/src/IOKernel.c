#include "../headers/IOKernel.h"

// Recibe una solicitud de IO desde una CPU
bool recv_IO_from_CPU(int fd, char** nombre_IO, int* cant_tiempo, int* PC) {
    int buffer_size;
    void* buffer = recibir_buffer(&buffer_size, fd);
    if (!buffer) {
        log_error(kernel_log, "recv_IO_from_CPU: Error al recibir el buffer de IO_OP");
        return false;
    }

    // Deserializar: primero string, luego int
    int offset = 0;
    *nombre_IO = leer_string(buffer, &offset);
    *cant_tiempo = leer_entero(buffer, &offset);
    *PC = leer_entero(buffer, &offset);

    log_trace(kernel_log, "recv_IO_from_CPU: nombre_IO='%s', tiempo=%d, PC=%d", *nombre_IO, *cant_tiempo, *PC);

    free(buffer);
    return true;
}

