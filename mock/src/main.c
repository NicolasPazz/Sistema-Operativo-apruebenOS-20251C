#include "../headers/mock.h"
#include "../headers/menu.h"

t_log* mock_log;
int fd_modulo_test = -1;
typedef struct {
    char* nombre;
    char* ip;
    char* puerto;
} modulo_info;

modulo_info obtener_info_modulo(char* nombre) {
    if (strcmp(nombre, "memoria") == 0) return (modulo_info){"memoria", "127.0.0.1", "8002"};
    if (strcmp(nombre, "kernel_dispatch") == 0) return (modulo_info){"kernel_dispatch", "127.0.0.1", "8001"};
    if (strcmp(nombre, "kernel_interrupt") == 0) return (modulo_info){"kernel_interrupt", "127.0.0.1", "8004"};
    if (strcmp(nombre, "kernel_io") == 0) return (modulo_info){"kernel_io", "127.0.0.1", "8003"};
    if (strcmp(nombre, "cpu_dispatch") == 0) return (modulo_info){"cpu_dispatch", "127.0.0.1", "8001"};
    if (strcmp(nombre, "cpu_interrupt") == 0) return (modulo_info){"cpu_interrupt", "127.0.0.1", "8004"};

    log_error(mock_log, "Módulo desconocido: %s", nombre);
    exit(EXIT_FAILURE);
}

int obtener_handshake(char* soy, char* destino) {
    if (strcmp(soy, "kernel") == 0 && strcmp(destino, "memoria") == 0) return HANDSHAKE_MEMORIA_KERNEL;
    if (strcmp(soy, "cpu") == 0 && strcmp(destino, "memoria") == 0) return HANDSHAKE_MEMORIA_CPU;
    if (strcmp(soy, "kernel") == 0 && strcmp(destino, "cpu_interrupt") == 0) return HANDSHAKE_CPU_KERNEL_INTERRUPT;
    if (strcmp(soy, "kernel") == 0 && strcmp(destino, "cpu_dispatch") == 0) return HANDSHAKE_CPU_KERNEL_DISPATCH;
    if (strcmp(soy, "kernel") == 0 && strcmp(destino, "kernel_io") == 0) return HANDSHAKE_IO_KERNEL;

    log_error(mock_log, "Handshake no definido para %s -> %s", soy, destino);
    exit(EXIT_FAILURE);
}

/////////////////////////////// Entrypoint ///////////////////////////////
int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Uso: %s [soy_modulo] [modulo_a_testear]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* soy = argv[1];
    char* testear = argv[2];

    mock_log = log_create("mock.log", "MOCK", true, LOG_LEVEL_INFO);

    modulo_info info = obtener_info_modulo(testear);
    fd_modulo_test = crear_conexion(info.ip, info.puerto, mock_log);

    int handshake = obtener_handshake(soy, testear);
    if (send(fd_modulo_test, &handshake, sizeof(int), 0) <= 0) {
        log_error(mock_log, "Error al enviar handshake a %s", testear);
        close(fd_modulo_test);
        exit(EXIT_FAILURE);
    }

    // Menú mock con fd del módulo a testear
    menu(fd_modulo_test);

    // Cierre de conexión
    close(fd_modulo_test);
    log_destroy(mock_log);
    return 0;
}

void iterator(char* value) {
    log_debug(mock_log, "%s", value);
}