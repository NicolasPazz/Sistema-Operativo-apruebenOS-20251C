#ifndef MENU_H
#define MENU_H

#include "../../utils/headers/sockets.h"
#include <commons/log.h>
#include "../headers/mock_ops.h"

extern t_log* mock_log;

/////////////////////////////// Prototipos ///////////////////////////////
void menu(int fd_a_testear);
void ejecutar_submenu(op_code cod, int fd_a_testear);

#endif /* MENU_H */
