#include "../headers/IOKernel.h"

// Busca un dispositivo IO por su nombre
io *get_io(char *nombre_io)
{
    io *dispositivo = NULL;

    if (!nombre_io)
    {
        log_trace(kernel_log, "Nombre de IO nulo");
        return NULL;
    }

    LOCK_CON_LOG(mutex_ios);

    for (int i = 0; i < list_size(lista_ios); i++)
    {
        io *io_actual = list_get(lista_ios, i);
        if (strcmp(io_actual->nombre, nombre_io) == 0)
        {
            dispositivo = io_actual;
            break;
        }
    }
    UNLOCK_CON_LOG(mutex_ios);

    log_trace(kernel_log, "IO '%s' %s encontrada", nombre_io, dispositivo ? "encontrada" : "no encontrada");

    return dispositivo;
}

io *buscar_io_por_fd(int fd)
{
    if (!lista_ios)
    {
        log_trace(kernel_log, "lista_ios es NULL");
        return NULL;
    }

    // Buscar el dispositivo IO por file descriptor
    for (int i = 0; i < list_size(lista_ios); i++)
    {
        io *disp = list_get(lista_ios, i);
        if (disp && disp->fd == fd)
        {
            return disp;
        }
    }

    // No se encontró el dispositivo
    log_trace(kernel_log, "No se encontró IO con fd %d", fd);
    return NULL;
}

io *buscar_io_por_nombre(char *nombre)
{
    if (!lista_ios || !nombre)
    {
        log_trace(kernel_log, "lista_ios o nombre es NULL");
        return NULL;
    }

    // Buscar el dispositivo IO por nombre
    for (int i = 0; i < list_size(lista_ios); i++)
    {
        io *disp = list_get(lista_ios, i);
        if (disp && strcmp(disp->nombre, nombre) == 0)
        {
            return disp;
        }
    }

    // No se encontró el dispositivo
    log_trace(kernel_log, "buscar_io_por_nombre: No se encontró IO con nombre '%s'", nombre);
    return NULL;
}

// Verifica si un dispositivo IO está disponible
bool esta_libre_io(io *dispositivo)
{
    return dispositivo->estado == IO_DISPONIBLE;
}

io *io_disponible(char *nombre)
{
    if (!nombre)
    {
        log_trace(kernel_log, "Nombre de IO nulo");
        return false;
    }
    
    LOCK_CON_LOG(mutex_ios);
    
    for (int i = 0; i < list_size(lista_ios); i++)
    {
        io *dispositivo = list_get(lista_ios, i);
        if (strcmp(dispositivo->nombre, nombre) == 0 && dispositivo->estado == IO_DISPONIBLE)
        {
            log_trace(kernel_log, "IO '%s' está disponible (fd=%d)", dispositivo->nombre, dispositivo->fd);
            UNLOCK_CON_LOG(mutex_ios);
            return dispositivo;
        }
    }
    UNLOCK_CON_LOG(mutex_ios);
    return NULL;
}

// Agrega un PCB a la lista de bloqueados por un dispositivo IO
void bloquear_pcb_por_io(char *nombre_io, t_pcb *pcb, int tiempo_a_usar)
{
    io *dispositivo = io_disponible(nombre_io);
    if (dispositivo)
    {
        enviar_io(dispositivo, pcb, tiempo_a_usar);
    }
    else
    {
        log_trace(kernel_log, "No hay IO disponible con el nombre '%s'", nombre_io);

        LOCK_CON_LOG(mutex_pcbs_esperando_io);

        t_pcb_io *pcb_io = malloc(sizeof(t_pcb_io));
        pcb_io->pcb = pcb;
        pcb_io->io = get_io(nombre_io);
        pcb_io->tiempo_a_usar = tiempo_a_usar;
        list_add(pcbs_esperando_io, pcb_io);

        UNLOCK_CON_LOG(mutex_pcbs_esperando_io);
        log_trace(kernel_log, "PCB PID=%d agregado a la lista de bloqueados por IO '%s' por %d ms", pcb->PID, nombre_io, tiempo_a_usar);
    }
}

void enviar_io(io *dispositivo, t_pcb *pcb, int tiempo_a_usar)
{
    dispositivo->estado = IO_OCUPADO;
    dispositivo->proceso_actual = pcb;

    // Crear paquete serializado
    t_paquete *paquete = crear_paquete_op(IO_OP);
    agregar_a_paquete(paquete, dispositivo->nombre, strlen(dispositivo->nombre) + 1);
    agregar_entero_a_paquete(paquete, tiempo_a_usar);
    agregar_entero_a_paquete(paquete, pcb->PID);

    enviar_paquete(paquete, dispositivo->fd);
    eliminar_paquete(paquete);

    log_trace(kernel_log, "Enviado PID=%d a IO '%s' por %d ms", pcb->PID, dispositivo->nombre, tiempo_a_usar);
}

void verificar_procesos_bloqueados(io *io)
{
    if (!io)
    {
        log_trace(kernel_log, "IO no válida");
        return;
    }

    LOCK_CON_LOG(mutex_pcbs_esperando_io);

    // Obtener el primer proceso esperando una IO con ese nombre
    t_pcb_io *pendiente = obtener_pcb_esperando_io(io->nombre);

    if (!pendiente)
    {
        log_trace(kernel_log, "No hay procesos pendientes para la IO '%s'", io->nombre);
        
        LOCK_CON_LOG(mutex_ios);
        
        io->estado = IO_DISPONIBLE;
        io->proceso_actual = NULL;
        UNLOCK_CON_LOG(mutex_ios);
        UNLOCK_CON_LOG(mutex_pcbs_esperando_io);
        return;
    }

    log_trace(kernel_log, "Asignando IO '%s' (fd=%d) al proceso PID=%d por %d ms", io->nombre, io->fd, pendiente->pcb->PID, pendiente->tiempo_a_usar);

    enviar_io(io, pendiente->pcb, pendiente->tiempo_a_usar);

    free(pendiente);

    UNLOCK_CON_LOG(mutex_pcbs_esperando_io);
}

t_pcb_io *obtener_pcb_esperando_io(char *nombre_io)
{
    // Obtener primer proceso esperando una IO con el nombre dado
    for (int i = 0; i < list_size(pcbs_esperando_io); i++)
    {
        t_pcb_io *pcb_io = list_get(pcbs_esperando_io, i);
        if (pcb_io->io && strcmp(pcb_io->io->nombre, nombre_io) == 0)
        {
            return list_remove(pcbs_esperando_io, i);
        }
    }
    return NULL;
}

void exit_procesos_relacionados(io *dispositivo)
{
    // Exit proceso usando esta instancia de io
    if (dispositivo->proceso_actual)
    {
        log_trace(kernel_log, "Proceso PID=%d ejecutando en IO desconectada, moviendo a EXIT", dispositivo->proceso_actual->PID);
        cambiar_estado_pcb_mutex(dispositivo->proceso_actual, EXIT_ESTADO);
        dispositivo->proceso_actual = NULL;
    }
    else
    {
        log_trace(kernel_log, "IO '%s' se desconectó sin proceso en ejecución", dispositivo->nombre);
    }

    if (list_is_empty(pcbs_esperando_io))
        return;

    // Verificar si hay otros dispositivos IO con el mismo nombre
    bool hay_otra_instancia = false;
    io *otra_io = NULL;

    LOCK_CON_LOG(mutex_ios);
    
    for (int j = 0; j < list_size(lista_ios); j++)
    {
        otra_io = list_get(lista_ios, j);
        if (otra_io != dispositivo && strcmp(otra_io->nombre, dispositivo->nombre) == 0)
        {
            hay_otra_instancia = true;
            break;
        }
    }
    UNLOCK_CON_LOG(mutex_ios);

    // Si no hay otras IO con el mismo nombre (LIBRES U OCUPADAS), mover los procesos afectados a EXIT
    if (!hay_otra_instancia)
    {
        t_list *pcbs_afectados = list_create();

        LOCK_CON_LOG(mutex_pcbs_esperando_io);
        
        int i = 0;
        while (i < list_size(pcbs_esperando_io))
        {
            t_pcb_io *pcb_io = list_get(pcbs_esperando_io, i);
            if (strcmp(pcb_io->io->nombre, dispositivo->nombre) == 0)
            {
                list_add(pcbs_afectados, pcb_io);
                list_remove(pcbs_esperando_io, i);
            }
            else
            {
                i++;
            }
        }
        UNLOCK_CON_LOG(mutex_pcbs_esperando_io);

        for (int i = 0; i < list_size(pcbs_afectados); i++)
        {
            t_pcb_io *pcb_io_actual = list_get(pcbs_afectados, i);
            log_trace(kernel_log, "Proceso PID=%d en IO desconectada, moviendo a EXIT", pcb_io_actual->pcb->PID);
            cambiar_estado_pcb_mutex(pcb_io_actual->pcb, EXIT_ESTADO);
            free(pcb_io_actual);
        }

        list_destroy(pcbs_afectados);
    }
}