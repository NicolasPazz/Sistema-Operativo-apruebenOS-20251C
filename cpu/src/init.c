#include "../headers/init.h"
#include "../headers/mmu.h"

/////////////////////////////// Inicializacion de variables globales ///////////////////////////////
t_log* cpu_log;
t_config* cpu_config;
int fd_memoria;
int fd_kernel_dispatch;
int fd_kernel_interrupt;

char* IP_MEMORIA;
char* PUERTO_MEMORIA;
char* IP_KERNEL;
char* PUERTO_KERNEL_DISPATCH;
char* PUERTO_KERNEL_INTERRUPT;
char* ENTRADAS_TLB;
char* REEMPLAZO_TLB;
char* ENTRADAS_CACHE;
char* REEMPLAZO_CACHE;
char* RETARDO_CACHE;
char* LOG_LEVEL;

void leer_config_cpu(const char *path_cfg) {

    cpu_config = iniciar_config((char *)path_cfg);

    IP_MEMORIA = config_get_string_value(cpu_config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_string_value(cpu_config, "PUERTO_MEMORIA");
    IP_KERNEL = config_get_string_value(cpu_config, "IP_KERNEL");
    PUERTO_KERNEL_DISPATCH = config_get_string_value(cpu_config, "PUERTO_KERNEL_DISPATCH");
    PUERTO_KERNEL_INTERRUPT = config_get_string_value(cpu_config, "PUERTO_KERNEL_INTERRUPT");
    ENTRADAS_TLB = config_get_string_value(cpu_config, "ENTRADAS_TLB");
    REEMPLAZO_TLB = config_get_string_value(cpu_config, "REEMPLAZO_TLB");
    ENTRADAS_CACHE = config_get_string_value(cpu_config, "ENTRADAS_CACHE");
    REEMPLAZO_CACHE = config_get_string_value(cpu_config, "REEMPLAZO_CACHE");
    RETARDO_CACHE = config_get_string_value(cpu_config, "RETARDO_CACHE");
    LOG_LEVEL = config_get_string_value(cpu_config, "LOG_LEVEL");

   /* Validación punteros obligatorios */
   if (!(IP_MEMORIA && PUERTO_MEMORIA &&
        IP_KERNEL && PUERTO_KERNEL_DISPATCH && PUERTO_KERNEL_INTERRUPT &&
        ENTRADAS_TLB && REEMPLAZO_TLB &&
        ENTRADAS_CACHE && REEMPLAZO_CACHE &&
        RETARDO_CACHE && LOG_LEVEL)) {
        fprintf(stderr, "leer_config_cpu: Faltan campos obligatorios en %s\n", path_cfg);
        exit(EXIT_FAILURE);
    }

    /* Prints resumen */
    printf("        Config leída: %s\n", path_cfg);
    printf("    IP_MEMORIA               : %s\n", IP_MEMORIA);
    printf("    PUERTO_MEMORIA           : %s\n", PUERTO_MEMORIA);
    printf("    IP_KERNEL                : %s\n", IP_KERNEL);
    printf("    PUERTO_KERNEL_DISPATCH   : %s\n", PUERTO_KERNEL_DISPATCH);
    printf("    PUERTO_KERNEL_INTERRUPT  : %s\n", PUERTO_KERNEL_INTERRUPT);
    printf("    ENTRADAS_TLB             : %s\n", ENTRADAS_TLB);
    printf("    REEMPLAZO_TLB            : %s\n", REEMPLAZO_TLB);
    printf("    ENTRADAS_CACHE           : %s\n", ENTRADAS_CACHE);
    printf("    REEMPLAZO_CACHE          : %s\n", REEMPLAZO_CACHE);
    printf("    RETARDO_CACHE            : %s\n", RETARDO_CACHE);
    printf("    LOG_LEVEL                : %s\n\n", LOG_LEVEL);
}

void iniciar_logger_cpu() {
    cpu_log = iniciar_logger("cpu/cpu.log", "CPU", 1, log_level_from_string(LOG_LEVEL));
    if (cpu_log == NULL) {
        printf("Error al iniciar cpu logs\n");
    } else {
        log_trace(cpu_log, "CPU logs iniciados correctamente!");
    }
}

void* conectar_cpu_memoria() {
    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, cpu_log);
    if (fd_memoria == -1) {
        log_error(cpu_log, "Error al conectar CPU a Memoria");
        exit(EXIT_FAILURE);
    }

    /* ────────────── 1. handshake ────────────── */
    int handshake = HANDSHAKE_MEMORIA_CPU;
    if (send(fd_memoria, &handshake, sizeof(int), 0) <= 0) {
        log_error(cpu_log, "Error al enviar handshake a Memoria: %s", strerror(errno));
        close(fd_memoria);
        exit(EXIT_FAILURE);
    }
    log_trace(cpu_log, "HANDSHAKE_MEMORIA_CPU: CPU conectado exitosamente a Memoria (fd=%d)", fd_memoria);

    /* ────────────── 2. pedir la configuración ────────────── */
    op_code op = PEDIR_CONFIG_CPU_OP;
    if (send(fd_memoria, &op, sizeof(op_code), 0) <= 0) {
        log_error(cpu_log, "Error al solicitar configuración a Memoria: %s", strerror(errno));
        close(fd_memoria);
        exit(EXIT_FAILURE);
    }

    int entradas_pt, tam_pagina, niveles;
    recv(fd_memoria, &entradas_pt, sizeof(int), MSG_WAITALL); // aca puede haber que usar paquete
    recv(fd_memoria, &tam_pagina,  sizeof(int), MSG_WAITALL); // aca podriamos usar paquete
    recv(fd_memoria, &niveles,     sizeof(int), MSG_WAITALL); // same

    /* ────────────── 3. guardar en cfg_memoria ────────────── */
    cfg_memoria = malloc(sizeof(t_config_memoria));
    cfg_memoria->ENTRADAS_POR_TABLA = entradas_pt;
    cfg_memoria->TAM_PAGINA         = tam_pagina;
    cfg_memoria->CANTIDAD_NIVELES   = niveles;
    /* los demás campos podés dejarlos en 0 o completarlos luego */
    
    log_trace(cpu_log,
        "Config recibida de Memoria ► Entradas/tabla=%d | Tamaño página=%d | Niveles=%d",
        entradas_pt, tam_pagina, niveles);

    return NULL;
}

void* conectar_kernel_dispatch() {
    fd_kernel_dispatch = crear_conexion(IP_KERNEL, PUERTO_KERNEL_DISPATCH, cpu_log);
    if (fd_kernel_dispatch == -1) {
        log_error(cpu_log, "Error al conectar CPU a Kernel Dispatch");
        exit(EXIT_FAILURE);
    }

    int handshake = HANDSHAKE_CPU_KERNEL_DISPATCH;
    if (send(fd_kernel_dispatch, &handshake, sizeof(int), 0) <= 0) {
        log_error(cpu_log, "Error al enviar handshake a Kernel Dispatch: %s", strerror(errno));
        close(fd_kernel_dispatch);
        exit(EXIT_FAILURE);
    }

    log_trace(cpu_log, "HANDSHAKE_CPU_KERNEL_DISPATCH: CPU conectado exitosamente a Kernel Dispatch (fd=%d)", fd_kernel_dispatch);
    return NULL;
}

void* conectar_kernel_interrupt() {
    fd_kernel_interrupt = crear_conexion(IP_KERNEL, PUERTO_KERNEL_INTERRUPT, cpu_log);
    if (fd_kernel_interrupt == -1) {
        log_error(cpu_log, "Error al conectar CPU a Kernel Interrupt");
        exit(EXIT_FAILURE);
    }

    int handshake = HANDSHAKE_CPU_KERNEL_INTERRUPT;
    if (send(fd_kernel_interrupt, &handshake, sizeof(int), 0) <= 0) {
        log_error(cpu_log, "Error al enviar handshake a Kernel Interrupt: %s", strerror(errno));
        close(fd_kernel_interrupt);
        exit(EXIT_FAILURE);
    }

    log_trace(cpu_log, "HANDSHAKE_CPU_KERNEL_INTERRUPT: CPU conectado exitosamente a Kernel Interrupt (fd=%d)", fd_kernel_interrupt);
    return NULL;
}