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

bool send_un_char_y_un_int(int fd, char* cadena, uint8_t entero);
bool send_dos_ints(int fd, uint8_t int1, uint8_t int2);

bool recv_un_char_y_un_int(int fd, char** cadena, uint8_t* entero);
bool recv_dos_ints(int fd, uint8_t* int1, uint8_t* int2);


#endif