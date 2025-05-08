#include "../headers/utils.h"
#include "../headers/sockets.h"

bool config_has_all_properties(t_config* cfg, char** properties) {
    for(uint8_t i = 0; properties[i] != NULL; i++) {
        if(!config_has_property(cfg, properties[i]))
            return false;
    }

    return true;
}

// ***** funciones para deserializar***

//definimos x protocolo q primero la cadena y desp el entero
static void deserializar_un_char_y_un_int (void* stream, char** cadena , uint8_t* entero){
    //string
    size_t size_cadena;
    memcpy(&size_cadena, stream, sizeof(size_t)); // guardo el tamanio de la cadena

    char* r_cadena = malloc(size_cadena); //asigno el tam
    memcpy(r_cadena, stream+sizeof(size_t), size_cadena);
    *cadena = r_cadena;
    
    //int
    memcpy(entero, stream+sizeof(size_t)+size_cadena ,sizeof(uint8_t));
}
static void deserializar_dos_ints(void* stream, uint8_t* int1, uint8_t* int2) {
    memcpy(int1, stream, sizeof(uint8_t));
    memcpy(int2, stream+sizeof(uint8_t), sizeof(uint8_t));
}

// ***** funciones para serializar***
static void* serializar_un_char_y_un_int(size_t* size, char* cadena, uint16_t entero) {
    size_t size_cadena = strlen(cadena) + 1;
    *size =
          sizeof(op_code)   // cop
        + sizeof(size_t)    // total
        + sizeof(size_t)    // size de char* cadena
        + size_cadena         // char* cadena
        + sizeof(uint16_t);  // entero
    size_t size_payload = *size - sizeof(op_code) - sizeof(size_t);

    void* stream = malloc(*size);

    op_code cop = MENSAJE_OP;
    memcpy(stream, &cop, sizeof(op_code));
    memcpy(stream+sizeof(op_code), &size_payload, sizeof(size_t));
    memcpy(stream+sizeof(op_code)+sizeof(size_t), &size_cadena, sizeof(size_t));
    memcpy(stream+sizeof(op_code)+sizeof(size_t)*2, cadena, size_cadena);
    memcpy(stream+sizeof(op_code)+sizeof(size_t)*2+size_cadena, &entero, sizeof(uint8_t));

    return stream;
}

//* probamos con 2 ints
static void* serializar_dos_ints(uint8_t int1, uint8_t int2) {
    void* stream = malloc(sizeof(op_code) + sizeof(uint8_t) * 2);

    op_code cop = PEDIR_INSTRUCCION_OP;
    memcpy(stream, &cop, sizeof(op_code));
    memcpy(stream+sizeof(op_code), &int1, sizeof(uint8_t));
    memcpy(stream+sizeof(op_code)+sizeof(uint8_t), &int2, sizeof(uint8_t));
    return stream;
}


//** ej de funciones de SEND -> serializan */

bool send_un_char_y_un_int(int fd, char* cadena, uint16_t entero) {
    size_t size;
    void* stream = serializar_un_char_y_un_int(&size, cadena, entero);
    if (send(fd, stream, size, 0) != size) {
        free(stream);
        return false;
    }
    free(stream);
    return true;
}

bool send_dos_ints(int fd, uint8_t int1, uint8_t int2) {
    size_t size = sizeof(op_code) + sizeof(uint8_t) * 2;
    void* stream = serializar_dos_ints(int1, int2);
    if (send(fd, stream, size, 0) != size) {
        free(stream);
        return false;
    }
    free(stream);
    return true;
}


//** ej funciones de RECV -> Deserializan */
bool recv_un_char_y_un_int(int fd, char** cadena, uint8_t* entero) {
    size_t size_payload;
    if (recv(fd, &size_payload, sizeof(size_t), 0) != sizeof(size_t))
        return false;

    void* stream = malloc(size_payload);
    if (recv(fd, stream, size_payload, 0) != size_payload) {
        free(stream);
        return false;
    }

    deserializar_un_char_y_un_int(stream, cadena, entero);

    free(stream);
    return true;
}

bool recv_dos_ints(int fd, uint8_t* int1, uint8_t* int2) {
    size_t size = sizeof(uint8_t) * 2;
    void* stream = malloc(size);

    if (recv(fd, stream, size, 0) != size) {
        free(stream);
        return false;
    }

    deserializar_dos_ints(stream, int1, int2);

    free(stream);
    return true;
}
// DEBUG
bool send_debug(int fd) {
    op_code cop = DEBUGGER;
    if (send(fd, &cop, sizeof(op_code), 0) != sizeof(op_code))
        return false;
    return true;
}