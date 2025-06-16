#ifndef SERIALIZACION_H
#define SERIALIZACION_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

/////////////////////////////////////// Envío por sockets ///////////////////////////////////////
bool enviar_entero(int socket, int p1);
bool enviar_2_enteros(int socket, int p1, int p2);
bool enviar_3_enteros(int socket, int p1, int p2, int p3);
bool enviar_4_enteros(int socket, int p1, int p2, int p3, int p4);

bool enviar_string(int socket, const char* s1);
bool enviar_3_string(int socket, const char* s1, const char* s2, const char* s3);

/////////////////////////////////////// Recepción por sockets ///////////////////////////////////////
bool recibir_entero(int socket, int* p1);
bool recibir_2_enteros(int socket, int* p1, int* p2);
bool recibir_3_enteros(int socket, int* p1, int* p2, int* p3);
bool recibir_4_enteros(int socket, int* p1, int* p2, int* p3, int* p4);

bool recibir_string(int socket, char** s1);
bool recibir_3_string(int socket, char** s1, char** s2, char** s3);

#endif /* SERIALIZACION_H */