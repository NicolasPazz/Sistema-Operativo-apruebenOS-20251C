#include "../headers/mock.h"
#include "../../utils/headers/utils.h"
#include "../../utils/headers/sockets.h"

/////////////////////////////// Funcionalidades ///////////////////////////////
bool INIT_PROC_OP_mock(int cliente_socket) {
    int pid;
    int tamanio;
    char instrucciones_path[256];  // Tamaño razonable para una ruta

    printf("PID del proceso: ");
    scanf("%d", &pid);
    if (!send_data(cliente_socket, &pid, sizeof(pid))) return false;

    printf("Tamaño del proceso: ");
    scanf("%d", &tamanio);
    if (!send_data(cliente_socket, &tamanio, sizeof(tamanio))) return false;

    printf("Ruta de instrucciones: ");
    scanf(" %[^\n]", instrucciones_path);  // Lee línea completa con espacios
    if (!send_string(cliente_socket, instrucciones_path)) return false;

    t_respuesta_memoria respuesta;
    if (recv(cliente_socket, &respuesta, sizeof(t_respuesta_memoria), 0) <= 0) {
        log_error(mock_log, "No se pudo recibir la respuesta del módulo.");
        return false;
    }

    if (respuesta == OK) {
        log_info(mock_log, "Proceso inicializado correctamente.");
    } else {
        log_warning(mock_log, "Error al inicializar el proceso.");
    }

    return true;
}

bool MENSAJE_OP_mock() { return true; }
bool PAQUETE_OP_mock() { return true; }
bool IO_OP_mock() { return true; }
bool DUMP_MEMORY_OP_mock() { return true; }
bool EXIT_OP_mock() { return true; }
bool EXEC_OP_mock() { return true; }
bool INTERRUPCION_OP_mock() { return true; }
bool PEDIR_INSTRUCCION_OP_mock() { return true; }
bool PEDIR_CONFIG_CPU_OP_mock() { return true; }
bool IO_FINALIZADA_OP_mock() { return true; }
bool FINALIZAR_PROC_OP_mock() { return true; }
bool DEBUGGER_mock() { return true; }
bool SEND_PSEUDOCOD_FILE_mock() { return true; }
bool NOOP_OP_mock() { return true; }
bool WRITE_OP_mock() { return true; }
bool READ_OP_mock() { return true; }
bool GOTO_OP_mock() { return true; }
bool PEDIR_PAGINA_OP_mock() { return true; }