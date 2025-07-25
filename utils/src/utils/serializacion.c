#include "../headers/serializacion.h"

// // // // // // // // // // // // // // // // // // // /Envío // // // // // // // // // // // // // // // // // // // /

bool enviar_entero(int socket, int p1)
{
    return send(socket, &p1, sizeof(int), 0) == sizeof(int);
}

bool enviar_2_enteros(int socket, int p1, int p2)
{
    return enviar_entero(socket, p1) && enviar_entero(socket, p2);
}

bool enviar_3_enteros(int socket, int p1, int p2, int p3)
{
    return enviar_entero(socket, p1) &&
           enviar_entero(socket, p2) &&
           enviar_entero(socket, p3);
}

bool enviar_4_enteros(int socket, int p1, int p2, int p3, int p4)
{
    return enviar_entero(socket, p1) &&
           enviar_entero(socket, p2) &&
           enviar_entero(socket, p3) &&
           enviar_entero(socket, p4);
}

bool enviar_string(int socket, const char *s1)
{
    int longitud = strlen(s1) + 1; // Incluye el '\0'
    if (!enviar_entero(socket, longitud))
        return false;
    return send(socket, s1, longitud, 0) == longitud;
}

bool enviar_3_string(int socket, const char *s1, const char *s2, const char *s3)
{
    return enviar_string(socket, s1) &&
           enviar_string(socket, s2) &&
           enviar_string(socket, s3);
}

// // // // // // // // // // // // // // // // // // // /Recepción // // // // // // // // // // // // // // // // // // // /

bool recibir_entero(int socket, int *p1)
{
    return recv(socket, p1, sizeof(int), MSG_WAITALL) == sizeof(int);
}

bool recibir_2_enteros(int socket, int *p1, int *p2)
{
    return recibir_entero(socket, p1) && recibir_entero(socket, p2);
}

bool recibir_3_enteros(int socket, int *p1, int *p2, int *p3)
{
    return recibir_entero(socket, p1) &&
           recibir_entero(socket, p2) &&
           recibir_entero(socket, p3);
}

bool recibir_4_enteros(int socket, int *p1, int *p2, int *p3, int *p4)
{
    return recibir_entero(socket, p1) &&
           recibir_entero(socket, p2) &&
           recibir_entero(socket, p3) &&
           recibir_entero(socket, p4);
}

// Reserva memoria dinámicamente
bool recibir_string(int socket, char **s1)
{
    int longitud;
    if (!recibir_entero(socket, &longitud))
        return false;

    char *buffer = malloc(longitud);
    if (!buffer)
        return false;

    if (recv(socket, buffer, longitud, MSG_WAITALL) != longitud)
    {
        free(buffer);
        return false;
    }

    *s1 = buffer;
    return true;
}

bool recibir_3_string(int socket, char **s1, char **s2, char **s3)
{
    return recibir_string(socket, s1) &&
           recibir_string(socket, s2) &&
           recibir_string(socket, s3);
}
