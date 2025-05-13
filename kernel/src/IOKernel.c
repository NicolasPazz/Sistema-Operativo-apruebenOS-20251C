#include "../headers/IOKernel.h"

// Procesa la petición de IO recibida de la CPU
void procesar_IO_from_CPU(char* nombre_IO, uint16_t cant_tiempo, t_pcb* pcb_a_io) {
    log_debug(kernel_log, "Procesando solicitud de IO '%s' por %d ms para PID=%d", 
              nombre_IO, cant_tiempo, pcb_a_io->PID);
    
    // Llamamos a la syscall IO con los parámetros correctos
    IO(nombre_IO, cant_tiempo, pcb_a_io);
}

// Recibe una solicitud de IO desde una CPU
bool recv_IO_from_CPU(int fd, char** nombre_IO, uint16_t* cant_tiempo) {
    op_code cop;
    if (recv(fd, &cop, sizeof(op_code), 0) <= 0 || cop != IO_OP) {
        log_error(kernel_log, "Error al recibir código de operación IO o código incorrecto");
        return false;
    }
    
    size_t size_payload;
    if (recv(fd, &size_payload, sizeof(size_t), 0) != sizeof(size_t)) {
        log_error(kernel_log, "Error al recibir tamaño del payload IO");
        return false;
    }

    void* stream = malloc(size_payload);
    if (recv(fd, stream, size_payload, 0) != size_payload) {
        log_error(kernel_log, "Error al recibir payload IO");
        free(stream);
        return false;
    }
<<<<<<< HEAD
    deserializar_un_char_y_un_int(stream, nombre_IO, cant_tiempo);
=======

    // Deserializar los datos
    size_t nombre_len;
    memcpy(&nombre_len, stream, sizeof(size_t));
    
    *nombre_IO = malloc(nombre_len);
    memcpy(*nombre_IO, stream + sizeof(size_t), nombre_len);
    
    memcpy(cant_tiempo, stream + sizeof(size_t) + nombre_len, sizeof(uint16_t));
>>>>>>> b1f878b7fcd43952b9a081935d83829ccfc219cc

    log_debug(kernel_log, "Recibido IO: nombre='%s', tiempo=%d", *nombre_IO, *cant_tiempo);
    
    free(stream);
    return true;
}
