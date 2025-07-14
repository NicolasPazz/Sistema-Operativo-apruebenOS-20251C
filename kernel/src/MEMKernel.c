#include "../headers/MEMKernel.h"
#include "../headers/kernel.h"

int conectar_memoria()
{
    log_trace(kernel_log, "Conectando a Memoria en %s:%s", IP_MEMORIA, PUERTO_MEMORIA);

    int fd = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, kernel_log);
    if (fd == -1)
    {
        log_error(kernel_log, "conectar_memoria: no se pudo abrir socket");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "conectar_memoria: socket abierto");

    int handshake = HANDSHAKE_MEMORIA_KERNEL;
    if (send(fd, &handshake, sizeof(handshake), 0) <= 0)
    {
        log_error(kernel_log, "conectar_memoria: fallo handshake");
        close(fd);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "conectar_memoria: fd=%d OK", fd);
    return fd;
}

void desconectar_memoria(int fd)
{
    if (fd != -1)
        close(fd);
}

bool inicializar_proceso_en_memoria(t_pcb *pcb)
{
    log_trace(kernel_log, "Inicializando proceso en Memoria: PID %d", pcb->PID);

    int fd = conectar_memoria();

    t_paquete *paq = crear_paquete_op(INIT_PROC_OP);
    agregar_a_paquete(paq, &pcb->PID, sizeof(int));
    agregar_a_paquete(paq, pcb->path, strlen(pcb->path) + 1);
    agregar_a_paquete(paq, &pcb->tamanio_memoria, sizeof(int));

    enviar_paquete(paq, fd);
    eliminar_paquete(paq);

    t_respuesta rsp;
    if (recv(fd, &rsp, sizeof(rsp), MSG_WAITALL) <= 0 ||
        (rsp != OK && rsp != ERROR))
    {
        log_error(kernel_log, "INIT_PROC_OP: respuesta inválida/timeout");
        desconectar_memoria(fd);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    desconectar_memoria(fd);

    if (rsp == OK)
    {
        log_trace(kernel_log, "INIT_PROC_OP: PID %d inicializado en Memoria", pcb->PID);
        return true;
    }

    log_trace(kernel_log, "INIT_PROC_OP: Memoria sin espacio para PID %d", pcb->PID);
    return false;
}

bool hay_espacio_suficiente_memoria(int tamanio)
{
    log_trace(kernel_log, "Verificando espacio suficiente en memoria para tamaño %d", tamanio);
    t_paquete *paquete = crear_paquete_op(CHECK_MEMORY_SPACE_OP);
    agregar_entero_a_paquete(paquete, tamanio);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    int respuesta = recibir_operacion(fd_memoria);
    if (respuesta < 0 || (respuesta != OK && respuesta != ERROR))
    {
        log_error(kernel_log, "Error al recibir respuesta de memoria");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    if (respuesta == OK)
    {
        log_trace(kernel_log, "Espacio suficiente en memoria para tamaño %d", tamanio);
        return true;
    }
    else if (respuesta == ERROR)
    {
        log_trace(kernel_log, "No hay espacio suficiente en memoria para tamaño %d", tamanio);
        return false;
    }
    else
    {
        log_error(kernel_log, "Respuesta inesperada de memoria: %d", respuesta);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
}

static bool enviar_op_memoria(int op_code, int pid)
{
    int fd = conectar_memoria();

    log_trace(kernel_log, "Enviando operación %d a Memoria para PID %d", op_code, pid);
    t_paquete *paq = crear_paquete_op(op_code);
    agregar_entero_a_paquete(paq, pid);

    enviar_paquete(paq, fd);
    eliminar_paquete(paq);

    t_respuesta rsp;
    if (recv(fd, &rsp, sizeof(rsp), MSG_WAITALL) <= 0 ||
        (rsp != OK && rsp != ERROR))
    {
        log_error(kernel_log, "Error al recibir respuesta de Memoria para OP %d y PID %d", op_code, pid);
        desconectar_memoria(fd);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    desconectar_memoria(fd);

    if (rsp == OK)
    {
        log_trace(kernel_log, "Operación %d exitosa para PID %d", op_code, pid);
        return true;
    }

    log_trace(kernel_log, "Operación %d fallida para PID %d", op_code, pid);
    return false;
}

bool suspender_proceso(t_pcb *pcb)
{
    return enviar_op_memoria(SUSPENDER_PROCESO_OP, pcb->PID);
}

bool desuspender_proceso(t_pcb *pcb)
{
    return enviar_op_memoria(DESUSPENDER_PROCESO_OP, pcb->PID);
}

bool finalizar_proceso_en_memoria(int pid)
{
    return enviar_op_memoria(FINALIZAR_PROC_OP, pid);
}

bool dump_memory(int pid)
{
    return enviar_op_memoria(DUMP_MEMORY_OP, pid);
}