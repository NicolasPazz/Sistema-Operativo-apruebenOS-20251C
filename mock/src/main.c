#include "../headers/mock.h"

t_log* mock_log;

int fd_memoria;
int fd_kernel_dispatch;
int fd_kernel_interrupt;
int fd_kernel_io;

int fd_cpu_dispatch;
int fd_cpu_interrupt;

/////////////////////////////// Menú Mock ///////////////////////////////
void menu() {
    int opcion;
    op_code cod;

    do {
        printf("\n===== MOCK TEST MENU =====\n");
        printf(" 1. MENSAJE_OP\n");
        printf(" 2. PAQUETE_OP\n");
        printf(" 3. IO_OP\n");
        printf(" 4. INIT_PROC_OP\n");
        printf(" 5. DUMP_MEMORY_OP\n");
        printf(" 6. EXIT_OP\n");
        printf(" 7. EXEC_OP\n");
        printf(" 8. INTERRUPCION_OP\n");
        printf(" 9. PEDIR_INSTRUCCION_OP\n");
        printf("10. PEDIR_CONFIG_CPU_OP\n");
        printf("11. IO_FINALIZADA_OP\n");
        printf("12. FINALIZAR_PROC_OP\n");
        printf("13. DEBUGGER\n");
        printf("14. SEND_PSEUDOCOD_FILE\n");
        printf("15. NOOP_OP\n");
        printf("16. WRITE_OP\n");
        printf("17. READ_OP\n");
        printf("18. GOTO_OP\n");
        printf("19. PEDIR_PAGINA_OP\n");
        printf("20. Salir\n");
        printf("Seleccioná una opción: ");
        scanf("%d", &opcion);

        if (opcion >= 1 && opcion <= 19) {
            cod = (op_code)(opcion - 1);

            // Elegís manualmente a quién enviar (en este caso, solo a Memoria)
            if (send(fd_memoria, &cod, sizeof(op_code), 0) <= 0) {
                log_error(mock_log, "Fallo al enviar op_code %d", cod);
            } else {
                log_info(mock_log, "op_code %d enviado correctamente a Memoria", cod);
            }
        } else if (opcion != 20) {
            printf("Opción inválida. Elegí entre 1 y 20.\n");
        }

    } while(opcion != 20);

    log_info(mock_log, "Saliendo del menú mock...");
}

// Entrypoint
int main(int argc, char* argv[]) {
    mock_log = log_create("mock.log", "MOCK", true, LOG_LEVEL_INFO);

    // IPs y puertos hardcodeados
    fd_memoria = crear_conexion("127.0.0.1", "8002", mock_log);
    // fd_kernel_dispatch = crear_conexion("127.0.0.1", "8001", mock_log);
    // fd_kernel_interrupt = crear_conexion("127.0.0.1", "8004", mock_log);
    // fd_kernel_io = crear_conexion("127.0.0.1", "8003", mock_log);
    // fd_cpu_dispatch = crear_conexion("127.0.0.1", "8001", mock_log);
    // fd_cpu_interrupt = crear_conexion("127.0.0.1", "8004", mock_log);

    int handshake = HANDSHAKE_MEMORIA_KERNEL;
    if (send(fd_memoria, &handshake, sizeof(int), 0) <= 0) {
        log_error(mock_log, "Error al enviar handshake a Memoria");
        close(fd_memoria);
        exit(EXIT_FAILURE);
    }

    menu();

    // Cierre de sockets
    close(fd_memoria);
    // close(fd_kernel_dispatch);
    // close(fd_kernel_interrupt);
    // close(fd_kernel_io);
    // close(fd_cpu_dispatch);
    // close(fd_cpu_interrupt);

    log_destroy(mock_log);
    return 0;
}

void iterator(char* value) {
    log_debug(mock_log, "%s", value);
}
