#include "../headers/MEMKernel.h"
#include "../headers/kernel.h"

int memoria_ocupada = 0;

int conectar_memoria()
{
    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Conectando a Memoria en %s:%s", IP_MEMORIA, PUERTO_MEMORIA);

    int fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, kernel_log);
    if (fd_memoria == -1)
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] no se pudo abrir socket");
        return -1;
    }

    LOCK_CON_LOG(mutex_sockets);
    list_add(lista_sockets, (void *)(intptr_t)fd_memoria);
    UNLOCK_CON_LOG(mutex_sockets);

    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] socket abierto");

    int handshake = HANDSHAKE_MEMORIA_KERNEL;
    if (send(fd_memoria, &handshake, sizeof(handshake), 0) <= 0)
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] fallo handshake");
        close(fd_memoria);
        return -1;
    }

    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] fd_memoria=%d OK", fd_memoria);
    return fd_memoria;
}

void desconectar_memoria(int fd_memoria)
{
    if (fd_memoria != -1)
    {
        close(fd_memoria);

        LOCK_CON_LOG(mutex_sockets);
        list_remove_element(lista_sockets, (void *)(intptr_t)fd_memoria);
        UNLOCK_CON_LOG(mutex_sockets);

        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Socket cerrado (fd=%d)", fd_memoria);
    }
}

bool inicializar_proceso_en_memoria(t_pcb *pcb)
{
    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Inicializando proceso en Memoria: PID %d", pcb->PID);

    /*if (!hay_espacio_suficiente_memoria(pcb->tamanio_memoria))
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] No hay espacio suficiente en memoria para PID %d", pcb->PID);
        return false;
    }*/

    int fd_memoria = conectar_memoria();

    t_paquete *paq = crear_paquete_op(INIT_PROC_OP);
    agregar_a_paquete(paq, &pcb->PID, sizeof(int));
    agregar_a_paquete(paq, pcb->path, strlen(pcb->path) + 1);
    agregar_a_paquete(paq, &pcb->tamanio_memoria, sizeof(int));

    enviar_paquete(paq, fd_memoria);
    eliminar_paquete(paq);

    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Esperando respuesta de Memoria para INIT_PROC_OP");

    t_respuesta rsp;
    if (recv(fd_memoria, &rsp, sizeof(rsp), MSG_WAITALL) <= 0 ||
        (rsp != OK && rsp != ERROR))
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] INIT_PROC_OP: respuesta inválida/timeout");
        desconectar_memoria(fd_memoria);
        return false;
    }

    desconectar_memoria(fd_memoria);

    if (rsp == OK)
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] INIT_PROC_OP: PID %d inicializado en Memoria", pcb->PID);
        if (strcmp(archivo_pseudocodigo, "PLANI_LYM_PLAZO") == 0)
        {
            memoria_ocupada += pcb->tamanio_memoria;
            int restante = 256 - memoria_ocupada;
            log_info(kernel_log, AMARILLO("## (%d) - %d B - Memoria ocupada: %d B ocupados, %d B libres"), pcb->PID, pcb->tamanio_memoria, memoria_ocupada, restante);
        }
        return true;
    }

    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] INIT_PROC_OP: Memoria sin espacio para PID %d", pcb->PID);
    return false;
}

bool hay_espacio_suficiente_memoria(int tamanio)
{
    int fd_memoria = conectar_memoria();

    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Verificando espacio suficiente en memoria para tamaño %d", tamanio);
    t_paquete *paquete = crear_paquete_op(CHECK_MEMORY_SPACE_OP);
    agregar_entero_a_paquete(paquete, tamanio);
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);

    int respuesta = recibir_operacion(fd_memoria);
    desconectar_memoria(fd_memoria);

    if (respuesta < 0 || (respuesta != OK && respuesta != ERROR))
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Error al recibir respuesta de memoria");
        return false;
    }

    if (respuesta == OK)
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Espacio suficiente en memoria para tamaño %d", tamanio);
        return true;
    }
    else if (respuesta == ERROR)
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] No hay espacio suficiente en memoria para tamaño %d", tamanio);
        return false;
    }
    else
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Respuesta inesperada de memoria: %d", respuesta);
        return false;
    }
    return false;
}

static bool enviar_op_memoria(int op_code, int pid)
{
    int fd_memoria = conectar_memoria();

    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Enviando operación %d a Memoria para PID %d", op_code, pid);
    t_paquete *paq = crear_paquete_op(op_code);
    agregar_entero_a_paquete(paq, pid);

    enviar_paquete(paq, fd_memoria);
    eliminar_paquete(paq);

    if ((op_code == SUSPENDER_PROCESO_OP) && strcmp(archivo_pseudocodigo, "PLANI_LYM_PLAZO") == 0)
    {
        t_pcb *pcb = buscar_pcb(pid);
        if (!pcb)
        {
            LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] No se encontró PCB para PID %d al finalizar/suspender", pid);
            return false;
        }
        memoria_ocupada -= pcb->tamanio_memoria;
        int restante = 256 - memoria_ocupada;
        log_info(kernel_log, AMARILLO("## (%d) - %d B - Memoria liberada: %d B ocupados, %d B libres"), pcb->PID, pcb->tamanio_memoria, memoria_ocupada, restante);
    }

    if (op_code == SUSPENDER_PROCESO_OP)
    {
        SEM_POST(sem_liberacion_memoria);
    }

    if (op_code != DESUSPENDER_PROCESO_OP)
    {
        t_respuesta rsp;
        if (recv(fd_memoria, &rsp, sizeof(rsp), MSG_WAITALL) <= 0 ||
            (rsp != OK && rsp != ERROR))
        {
            LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Error al recibir respuesta de Memoria para OP %d y PID %d", op_code, pid);
            desconectar_memoria(fd_memoria);
            return false;
        }

        desconectar_memoria(fd_memoria);

        if (rsp == OK)
        {
            LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Operación %d exitosa para PID %d", op_code, pid);

            if ((op_code == FINALIZAR_PROC_OP) && strcmp(archivo_pseudocodigo, "PLANI_LYM_PLAZO") == 0)
            {
                t_pcb *pcb = buscar_pcb(pid);
                if (!pcb)
                {
                    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] No se encontró PCB para PID %d al finalizar/suspender", pid);
                    return false;
                }
                memoria_ocupada -= pcb->tamanio_memoria;
                int restante = 256 - memoria_ocupada;
                log_info(kernel_log, AMARILLO("## (%d) - %d B - Memoria liberada: %d B ocupados, %d B libres"), pcb->PID, pcb->tamanio_memoria, memoria_ocupada, restante);
            }
            else if (op_code == DESUSPENDER_PROCESO_OP && strcmp(archivo_pseudocodigo, "PLANI_LYM_PLAZO") == 0)
            {
                t_pcb *pcb = buscar_pcb(pid);
                if (!pcb)
                {
                    LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] No se encontró PCB para PID %d al finalizar/suspender", pid);
                    return false;
                }
                memoria_ocupada += pcb->tamanio_memoria;
                int restante = 256 - memoria_ocupada;
                log_info(kernel_log, AMARILLO("## (%d) - %d B - Memoria ocupada: %d B ocupados, %d B libres"), pcb->PID, pcb->tamanio_memoria, memoria_ocupada, restante);
            }

            return true;
        }

        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] Operación %d fallida para PID %d", op_code, pid);
        return false;
    }
    return true;
}

bool suspender_proceso(t_pcb *pcb)
{
    return enviar_op_memoria(SUSPENDER_PROCESO_OP, pcb->PID);
}

bool desuspender_proceso(t_pcb *pcb)
{
    if (!hay_espacio_suficiente_memoria(pcb->tamanio_memoria))
    {
        LOG_TRACE(kernel_log, "[KERNEL->MEMORIA] No hay espacio suficiente en memoria para desuspender PID %d", pcb->PID);
        return false;
    }
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