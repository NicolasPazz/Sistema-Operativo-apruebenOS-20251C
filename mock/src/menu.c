#include "../headers/menu.h"

/////////////////////////////// Menú de Mockitos 🍵 ///////////////////////////////
void ejecutar_submenu(op_code cod, int fd_a_testear) {
    int sub_opcion;

    do {
        printf("\n--- Submenú para operación %d ---\n", cod);
        printf("1. Ejecutar operación\n");
        printf("2. Volver\n");
        printf("Seleccioná una opción: ");
        scanf("%d", &sub_opcion);

        switch (sub_opcion) {
            case 1:
                switch (cod) {
                    case MENSAJE_OP:
                        MENSAJE_OP_mock(); break;
                    case PAQUETE_OP:
                        PAQUETE_OP_mock(); break;
                    case IO_OP:
                        IO_OP_mock(); break;
                    case INIT_PROC_OP:
                        INIT_PROC_OP_mock(fd_a_testear); break;
                    case DUMP_MEMORY_OP:
                        DUMP_MEMORY_OP_mock(); break;
                    case EXIT_OP:
                        EXIT_OP_mock(); break;
                    case EXEC_OP:
                        EXEC_OP_mock(); break;
                    case INTERRUPCION_OP:
                        INTERRUPCION_OP_mock(); break;
                    case PEDIR_INSTRUCCION_OP:
                        PEDIR_INSTRUCCION_OP_mock(fd_a_testear); break;
                    case PEDIR_CONFIG_CPU_OP:
                        PEDIR_CONFIG_CPU_OP_mock(); break;
                    case IO_FINALIZADA_OP:
                        IO_FINALIZADA_OP_mock(); break;
                    case FINALIZAR_PROC_OP:
                        FINALIZAR_PROC_OP_mock(); break;
                    case DEBUGGER:
                        DEBUGGER_mock(); break;
                    case SEND_PSEUDOCOD_FILE:
                        SEND_PSEUDOCOD_FILE_mock(); break;
                    case NOOP_OP:
                        NOOP_OP_mock(); break;
                    case WRITE_OP:
                        WRITE_OP_mock(); break;
                    case READ_OP:
                        READ_OP_mock(); break;
                    case GOTO_OP:
                        GOTO_OP_mock(); break;
                    case PEDIR_PAGINA_OP:
                        PEDIR_PAGINA_OP_mock(); break;
                    default:
                        printf("Operación aún no implementada.\n");
                }
                break;
            case 2:
                printf("Volviendo al menú principal...\n");
                break;
            default:
                printf("Opción inválida. Elegí entre 1 y 2.\n");
        }

    } while (sub_opcion != 2);
}

void menu(int fd_a_testear) {
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

            // FD del Socker a testear con los OP_CODE
            if (send(fd_a_testear, &cod, sizeof(op_code), 0) <= 0) {
                log_error(mock_log, "Fallo al enviar op_code %d", cod);
            } else {
                log_trace(mock_log, "op_code %d enviado correctamente a Memoria", cod);
            }

            // Menú de cada op_code
            ejecutar_submenu(cod, fd_a_testear);

        } else if (opcion != 20) {
            printf("Opción inválida. Elegí entre 1 y 20.\n");
        }

    } while (opcion != 20);

    log_trace(mock_log, "Saliendo del menú mock...");
}
