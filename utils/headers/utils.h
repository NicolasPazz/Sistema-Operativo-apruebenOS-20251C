#ifndef UTILS_H_
#define UTILS_H_

#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/string.h>

bool config_has_all_properties(t_config*, char**);

bool send_un_char_y_un_int(int fd, char* cadena, int entero);
bool send_dos_ints(int fd, int int1, int int2);
bool send_string(int fd, char* string);
bool send_data(int fd, void* data, size_t size);

bool recv_un_char_y_un_int(int fd, char** cadena, int* entero);
bool recv_dos_ints(int fd, int* int1, int* int2);
bool recv_string(int fd, char** string);
bool recv_data(int fd, void* buffer, size_t size);


#endif /* UTILS_H_ */