#include "../headers/utils.h"
#include "../headers/sockets.h"

bool config_has_all_properties(t_config* cfg, char** properties) {
    for(int i = 0; properties[i] != NULL; i++) {
        if(!config_has_property(cfg, properties[i]))
            return false;
    }

    return true;
}

// Deserialización
void deserializar_un_char_y_un_int (void* stream, char** cadena , int* entero){
    //string
    size_t size_cadena;
    memcpy(&size_cadena, stream, sizeof(size_t)); // guardo el tamanio de la cadena

    char* r_cadena = malloc(size_cadena); //asigno el tam
    memcpy(r_cadena, stream+sizeof(size_t), size_cadena);
    *cadena = r_cadena;
    
    //int
    memcpy(entero, stream+sizeof(size_t)+size_cadena ,sizeof(int));
}

void deserializar_dos_ints(void* stream, int* int1, int* int2) {
    memcpy(int1, stream, sizeof(int));
    memcpy(int2, stream+sizeof(int), sizeof(int));
}

// ***** funciones para serializar***
void* serializar_un_char_y_un_int(size_t* size, char* cadena, int entero) {
    size_t size_cadena = strlen(cadena) + 1;
    *size =
          sizeof(op_code)   // cop
        + sizeof(size_t)    // total
        + sizeof(size_t)    // size de char* cadena
        + size_cadena         // char* cadena
        + sizeof(int);  // entero
    size_t size_payload = *size - sizeof(op_code) - sizeof(size_t);

    void* stream = malloc(*size);

    op_code cop = MENSAJE_OP;
    memcpy(stream, &cop, sizeof(op_code));
    memcpy(stream+sizeof(op_code), &size_payload, sizeof(size_t));
    memcpy(stream+sizeof(op_code)+sizeof(size_t), &size_cadena, sizeof(size_t));
    memcpy(stream+sizeof(op_code)+sizeof(size_t)*2, cadena, size_cadena);
    memcpy(stream+sizeof(op_code)+sizeof(size_t)*2+size_cadena, &entero, sizeof(int));

    return stream;
}

//* probamos con 2 ints
void* serializar_dos_ints(int int1, int int2) {
    void* stream = malloc(sizeof(op_code) + sizeof(int) * 2);

    op_code cop = PEDIR_INSTRUCCION_OP;
    memcpy(stream, &cop, sizeof(op_code));
    memcpy(stream+sizeof(op_code), &int1, sizeof(int));
    memcpy(stream+sizeof(op_code)+sizeof(int), &int2, sizeof(int));
    return stream;
}


//** ej de funciones de SEND -> serializan */

bool send_un_char_y_un_int(int fd, char* cadena, int entero) {
    size_t size;
    void* stream = serializar_un_char_y_un_int(&size, cadena, entero);
    if (send(fd, stream, size, 0) != size) {
        free(stream);
        return false;
    }
    free(stream);
    return true;
}

bool send_dos_ints(int fd, int int1, int int2) {
    size_t size = sizeof(op_code) + sizeof(int) * 2;
    void* stream = serializar_dos_ints(int1, int2);
    if (send(fd, stream, size, 0) != size) {
        free(stream);
        return false;
    }
    free(stream);
    return true;
}

// Serializar y enviar un string
bool send_string(int fd, char* string) {
    // Calcular tamaño del mensaje: tamaño del string + 1 para el null terminator
    size_t string_length = strlen(string) + 1;
    
    // Enviar el tamaño del string primero
    if (send(fd, &string_length, sizeof(size_t), 0) != sizeof(size_t)) {
        return false;
    }
    
    // Enviar el contenido del string
    if (send(fd, string, string_length, 0) != string_length) {
        return false;
    }
    
    return true;
}

// Enviar datos genéricos
bool send_data(int fd, void* data, size_t size) {
    return send(fd, data, size, 0) == size;
}


//** ej funciones de RECV -> Deserializan */
bool recv_un_char_y_un_int(int fd, char** cadena, int* entero) {
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

bool recv_dos_ints(int fd, int* int1, int* int2) {
    size_t size = sizeof(int) * 2;
    void* stream = malloc(size);

    if (recv(fd, stream, size, 0) != size) {
        free(stream);
        return false;
    }

    deserializar_dos_ints(stream, int1, int2);

    free(stream);
    return true;
}

// Recibir un string 
bool recv_string(int fd, char** string) {
    // Recibir el tamaño del string
    size_t string_length;
    if (recv(fd, &string_length, sizeof(size_t), 0) != sizeof(size_t)) {
        return false;
    }
    
    // Reservar memoria para el string
    *string = malloc(string_length);
    if (*string == NULL) {
        return false;
    }
    
    // Recibir el contenido del string
    if (recv(fd, *string, string_length, 0) != string_length) {
        free(*string);
        *string = NULL;
        return false;
    }
    
    return true;
}

// Recibir datos genéricos
bool recv_data(int fd, void* buffer, size_t size) {
    return recv(fd, buffer, size, 0) == size;
}

// DEBUG
bool send_debug(int fd) {
    op_code cop = DEBUGGER;
    if (send(fd, &cop, sizeof(op_code), 0) != sizeof(op_code))
        return false;
    return true;
}