#include "../headers/planificadores.h"

void *planificador_corto_plazo(void *arg)
{
    log_trace(kernel_log, "=== PLANIFICADOR CP INICIADO ===");

    while (1)
    {

        if (strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0)
        {
            log_debug(kernel_log, "Planificador CP: semaforo REPLANIFICAR SRT disminuido");
            sem_wait(&sem_replanificar_srt);
        }
        else
        {
            // Esperar a que llegue un proceso a READY
            log_debug(kernel_log, "planificador_corto_plazo: Semaforo a READY disminuido");
            sem_wait(&sem_proceso_a_ready);
            // Esperar cpu disponible
            log_trace(kernel_log, "Planificador CP: ✓ Proceso llegó a READY - Verificando disponibilidad de cpu");
            log_debug(kernel_log, "planificador_corto_plazo: Semaforo CPU DISPONIBLE disminuido");
            sem_wait(&sem_cpu_disponible);
        }
        log_trace(kernel_log, "Planificador CP: ✓ Condiciones cumplidas - Iniciando planificación");
        t_pcb *proceso_elegido;

        if (strcmp(ALGORITMO_CORTO_PLAZO, "FIFO") == 0)
        {
            proceso_elegido = elegir_por_fifo();
        }
        else if (strcmp(ALGORITMO_CORTO_PLAZO, "SJF") == 0)
        {
            proceso_elegido = elegir_por_sjf();
        }
        else if (strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0)
        {
            proceso_elegido = elegir_por_srt();
        }
        else
        {
            log_error(kernel_log, "iniciar_planificador_corto_plazo: Algoritmo no reconocido");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        if (proceso_elegido)
        {
            dispatch(proceso_elegido);
            continue;
        }
        else if (strcmp(ALGORITMO_CORTO_PLAZO, "SRT") != 0)
        {
            log_error(kernel_log, "planificador_corto_plazo: No se pudo elegir un proceso para ejecutar");
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        log_trace(kernel_log, "planificador_corto_plazo: No se pudo elegir un proceso para ejecutar para SRT");
    }
}

t_pcb *elegir_por_sjf()
{
    log_trace(kernel_log, "PLANIFICANDO SJF (Shortest Job First)");

    log_debug(kernel_log, "SJF: esperando mutex_cola_ready para elegir proceso con menor rafaga");
    pthread_mutex_lock(&mutex_cola_ready);
    log_debug(kernel_log, "SJF: bloqueando mutex_cola_ready para elegir proceso con menor rafaga");
    if (list_is_empty(cola_ready))
    {
        pthread_mutex_unlock(&mutex_cola_ready);
        log_error(kernel_log, "SJF: cola_ready vacía");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "SJF: buscando entre %d procesos con menor rafaga en cola_ready", list_size(cola_ready));
    for (int i = 0; i < list_size(cola_ready); i++)
    {
        mostrar_pcb((t_pcb *)list_get(cola_ready, i));
    }

    t_pcb *seleccionado = (t_pcb *)list_get_minimum(cola_ready, menor_rafaga);
    pthread_mutex_unlock(&mutex_cola_ready);

    if (seleccionado)
    {
        log_trace(kernel_log, "SJF: Proceso elegido PID=%d con estimación=%.3f", seleccionado->PID, seleccionado->estimacion_rafaga);
    }
    else
    {
        log_error(kernel_log, "SJF: No se pudo seleccionar un proceso");
    }

    return seleccionado;
}

void *menor_rafaga(void *a, void *b)
{
    t_pcb *pcb_a = (t_pcb *)a;
    t_pcb *pcb_b = (t_pcb *)b;

    // Devuelve el de menor estimación de rafaga
    if (pcb_a->estimacion_rafaga < pcb_b->estimacion_rafaga)
        return pcb_a;
    if (pcb_b->estimacion_rafaga < pcb_a->estimacion_rafaga)
        return pcb_b;

    // En caso de empate, devolver el primero que llegó (fifo)
    return pcb_a;
}

t_pcb *elegir_por_srt()
{
    log_trace(kernel_log, "elegir_por_srt: PLANIFICANDO SRT (Shortest Remaining Time)");

    log_debug(kernel_log, "elegir_por_srt: esperando mutex_cola_ready para elegir proceso con menor rafaga restante");
    pthread_mutex_lock(&mutex_cola_ready);
    log_debug(kernel_log, "elegir_por_srt: bloqueando mutex_cola_ready para elegir proceso con menor rafaga");

    if (list_is_empty(cola_ready))
    {
        pthread_mutex_unlock(&mutex_cola_ready);
        log_error(kernel_log, "SRT: cola_ready vacía");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Buscar el proceso READY con menor rafaga restante
    t_pcb *candidato_ready = (t_pcb *)list_get_minimum(cola_ready, menor_rafaga_restante);
    pthread_mutex_unlock(&mutex_cola_ready);

    if (!candidato_ready)
    {
        log_error(kernel_log, "SRT: No se pudo seleccionar un proceso READY");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    log_trace(kernel_log, "SRT: Proceso candidato elegido PID=%d con rafaga restante=%.3f ms", candidato_ready->PID, candidato_ready->estimacion_rafaga);

    log_debug(kernel_log, "SRT: esperando mutex_lista_cpus para buscar CPU disponible o con mayor rafaga restante");
    pthread_mutex_lock(&mutex_lista_cpus);
    log_debug(kernel_log, "SRT: bloqueando mutex_lista_cpus para buscar CPU disponible o con mayor rafaga restante");

    bool cpu_libre = false;
    bool cpu_con_mayor_rafaga_restante = false;

    // Buscar CPUs disponibles y calcular cuál ejecuta el proceso con mayor rafaga restante
    for (int i = 0; i < list_size(lista_cpus); i++)
    {
        cpu *c = list_get(lista_cpus, i);
        if (c->tipo_conexion != CPU_DISPATCH)
            continue;

        // Verificar si la CPU está libre (pid = -1)
        if (c->pid == -1)
        {
            cpu_libre = true;
            break;     // hay una CPU libre
        }
    }
    if (!cpu_libre)
    {
        log_trace(kernel_log, "SRT: No hay CPU libre, verificando ráfagas restantes");
        for (int i = 0; i < list_size(lista_cpus); i++)
        {
            cpu *c = list_get(lista_cpus, i);
            if (c->tipo_conexion != CPU_DISPATCH)
                continue;

            // Si no está libre, buscar si al menos una tiene mayor rafaga restante que candidato_ready
            t_pcb *pcb_exec = buscar_pcb(c->pid);

            if (menor_rafaga_restante((void *)pcb_exec, (void *)candidato_ready) == (void *)candidato_ready)
            {
                // Esta CPU tiene un proceso con mayor rafaga restante que el candidato
                cpu_con_mayor_rafaga_restante = true;
                break;
            }
        }
    }

    pthread_mutex_unlock(&mutex_lista_cpus);

    // Si hay CPU libre o hay una CPU ejecutando un proceso con mayor rafaga restante
    if (cpu_libre)
    {
        log_trace(kernel_log, "SRT: Hay CPU libre para ejecutar el proceso READY seleccionado (PID=%d)", candidato_ready->PID);
        return candidato_ready;     // Se puede asignar el proceso ahora
    }
    else if (cpu_con_mayor_rafaga_restante)
    {
        log_trace(kernel_log, "SRT: Hay CPU ocupada con un proceso con mayor rafaga restante que el proceso READY seleccionado (PID=%d)", candidato_ready->PID);
        return candidato_ready;     // Se puede asignar el proceso ahora, pero se deberadesalojar
    }
    else
    { // Si no hay CPU libre ni una ejecutando un proceso con mayor rafaga restante
        // replanificar cuando haya una cpu libre o entre un proceso en ready?
        log_trace(kernel_log, "SRT: No hay CPU libre ni con mayor rafaga restante que el proceso READY seleccionado (PID=%d), se reintentará cuando se cumplan las condiciones", candidato_ready->PID);
        return NULL;     // No se puede asignar el proceso ahora
    }
}

void *menor_rafaga_restante(void *a, void *b)
{
    t_pcb *pcb_a = (t_pcb *)a;
    t_pcb *pcb_b = (t_pcb *)b;

    // Calcular rafaga restante
    double restante_a;
    double restante_b;
    double ahora = get_time();

    if (pcb_a->tiempo_inicio_exec > 0)
    {
        restante_a = pcb_a->estimacion_rafaga - (ahora - pcb_a->tiempo_inicio_exec);
    }
    else
    {
        restante_a = pcb_a->estimacion_rafaga;
    }

    if (pcb_b->tiempo_inicio_exec > 0)
    {
        restante_b = pcb_b->estimacion_rafaga - (ahora - pcb_b->tiempo_inicio_exec);
    }
    else
    {
        restante_b = pcb_b->estimacion_rafaga;
    }

    // Log de comparación
    log_trace(kernel_log, "Comparando procesos:");
    log_trace(kernel_log, "  • PID %d - Estado: %s - Ráfaga restante: %.3f ms", pcb_a->PID, estado_to_string(pcb_a->Estado), restante_a);
    log_trace(kernel_log, "  • PID %d - Estado: %s - Ráfaga restante: %.3f ms", pcb_b->PID, estado_to_string(pcb_b->Estado), restante_b);

    // Comparar ráfagas restantes
    if (restante_a < restante_b)
    {
        log_trace(kernel_log, "PID %d tiene menor rafaga restante (%.3f ms) que PID %d (%.3f ms)", pcb_a->PID, restante_a, pcb_b->PID, restante_b);
        return pcb_a;
    }
    if (restante_b < restante_a)
    {
        log_trace(kernel_log, "PID %d tiene menor rafaga restante (%.3f ms) que PID %d (%.3f ms)", pcb_b->PID, restante_b, pcb_a->PID, restante_a);
        return pcb_b;
    }

    // En caso de empate, devolver el primero que llegó (FIFO)
    log_trace(kernel_log, "Empate de rafaga restante entre PID %d y PID %d. Se elige FIFO (PID %d)", pcb_a->PID, pcb_b->PID, pcb_a->PID);
    return pcb_a;
}

void dispatch(t_pcb *proceso_a_ejecutar)
{
    log_trace(kernel_log, "=== DISPATCH INICIADO PARA PID %d ===", proceso_a_ejecutar->PID);

    // Buscar una CPU disponible (con pid = -1 indica que está libre)
    log_debug(kernel_log, "Dispatch: esperando mutex_lista_cpus para buscar CPU disponible");
    pthread_mutex_lock(&mutex_lista_cpus);
    log_debug(kernel_log, "Dispatch: bloqueando mutex_lista_cpus para buscar CPU disponible");

    cpu *cpu_disponible = NULL;
    int total_cpus = list_size(lista_cpus);
    int cpus_dispatch = 0;
    int cpus_libres = 0;

    for (int i = 0; i < total_cpus; i++)
    {
        cpu *c = list_get(lista_cpus, i);
        if (c->tipo_conexion == CPU_DISPATCH)
        {
            cpus_dispatch++;
            if (c->pid == -1)
            {
                cpus_libres++;
                if (!cpu_disponible)
                {
                    cpu_disponible = c;
                    log_trace(kernel_log, "Dispatch: ✓ CPU %d seleccionada (fd=%d)", c->id, c->fd);
                }
            }
        }
        log_trace(kernel_log, "Dispatch: CPU %d - tipo=%d, pid=%d, fd=%d, estado=%s", c->id, c->tipo_conexion, c->pid, c->fd, c->tipo_conexion == CPU_DISPATCH ? (c->pid == -1 ? "LIBRE" : "OCUPADA") : "NO-DISPATCH");
    }

    log_trace(kernel_log, "Dispatch: Total CPUs=%d, CPUs DISPATCH=%d, CPUs libres=%d", total_cpus, cpus_dispatch, cpus_libres);

    if (!cpu_disponible)
    {
        if (strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0)
        {
            // Buscar cpu con mayor rafaga restante
            double max_rafaga_restante = -1;

            for (int i = 0; i < list_size(lista_cpus); i++)
            {
                cpu *c = list_get(lista_cpus, i);
                if (c->tipo_conexion != CPU_DISPATCH)
                    continue;

                t_pcb *pcb_exec = buscar_pcb(c->pid);

                double rafaga_restante;

                if (pcb_exec->tiempo_inicio_exec > 0)
                {
                    rafaga_restante = pcb_exec->estimacion_rafaga - (get_time() - pcb_exec->tiempo_inicio_exec);
                }
                else
                {
                    log_error(kernel_log, "Dispatch: Error al calcular rafaga restante para PID %d (tiempo_inicio_exec no inicializado)", pcb_exec->PID);
                    terminar_kernel();
                    exit(EXIT_FAILURE);
                }

                if (rafaga_restante > max_rafaga_restante)
                {
                    max_rafaga_restante = rafaga_restante;
                    cpu_disponible = c;
                }
            }
            // Desalojar
            /*if(!interrupt(cpu_disponible, proceso_a_ejecutar)) {
                log_error(kernel_log, "Dispatch: ✗ Error al enviar interrupción a CPU %d para desalojar PID %d", cpu_disponible->id, proceso_a_ejecutar->PID);
                terminar_kernel();
                exit(EXIT_FAILURE);
            }
            // Continuar despachando el proceso a la CPU desalojada*/

            t_interrupcion *nueva = malloc(sizeof(t_interrupcion));
            nueva->cpu_a_desalojar = cpu_disponible;
            nueva->pid_a_ejecutar = proceso_a_ejecutar->PID;

            log_debug(kernel_log, "DISPATCH: esperando mutex_cola_interrupciones para encolar interrupción");
            pthread_mutex_lock(&mutex_cola_interrupciones);
            log_debug(kernel_log, "DISPATCH: bloqueando mutex_cola_interrupciones para encolar interrupción");
            queue_push(cola_interrupciones, nueva);
            pthread_mutex_unlock(&mutex_cola_interrupciones);
            log_trace(kernel_log, "[INTERRUPT]: Interrupción encolada para desalojar CPU %d (desaloja PID=%d para correr PID=%d)", cpu_disponible->id, cpu_disponible->pid, // PID actual en esa CPU
                      proceso_a_ejecutar->PID // nuevo PID a ejecutar
            );

            sem_post(&sem_interrupciones);
            log_debug(kernel_log, "[INTERRUPT]: semaforo INTERRUPCIONES aumentado para procesar interrupción");

            pthread_mutex_unlock(&mutex_lista_cpus);
            return;     // Esperar a que se procese la interrupción
        }
        else
        {
            pthread_mutex_unlock(&mutex_lista_cpus);
            log_error(kernel_log, "Dispatch: ✗ No hay CPUs disponibles para ejecutar PID %d", proceso_a_ejecutar->PID);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
    }

    // Marcar CPU como ocupada y guardar PID
    cpu_disponible->pid = proceso_a_ejecutar->PID;
    cpu_disponible->instruccion_actual = EXEC_OP;
    pthread_mutex_unlock(&mutex_lista_cpus);

    // Transicionar a EXEC
    cambiar_estado_pcb(proceso_a_ejecutar, EXEC);

    // Crear y enviar paquete a CPU
    t_paquete *paquete = crear_paquete_op(EXEC_OP);
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PC);
    agregar_entero_a_paquete(paquete, proceso_a_ejecutar->PID);
    enviar_paquete(paquete, cpu_disponible->fd);
    eliminar_paquete(paquete);

    log_trace(kernel_log, "Dispatch: Proceso %d despachado a CPU %d (PC=%d)", proceso_a_ejecutar->PID, cpu_disponible->id, proceso_a_ejecutar->PC);
}

void solicitar_replanificacion_srt(void)
{

    if (strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0 && list_size(cola_ready) > 0)
    {
        sem_post(&sem_replanificar_srt);
        log_debug(kernel_log, "solicitar_replanificacion_srt: semaforo replanificar SRT aumentado");
        log_trace(kernel_log, "[PLANI CP]: Replanificación SRT solicitada");
    }
}

void iniciar_interrupt_handler(void)
{
    // Crear hilo para manejar interrupciones
    pthread_t hilo_interrupt_handler;
    if (pthread_create(&hilo_interrupt_handler, NULL, interrupt_handler, NULL) != 0)
    {
        log_error(kernel_log, "Error al crear hilo para manejar interrupciones");
        terminar_kernel();
        exit(EXIT_FAILURE);
    }
    pthread_detach(hilo_interrupt_handler);
    log_trace(kernel_log, "Hilo de manejo de interrupciones iniciado correctamente");
}

void *interrupt_handler(void *arg)
{
    log_trace(kernel_log, VERDE("Interrupt handler iniciado"));

    while (1)
    {
        log_debug(kernel_log, "[INTERRUPT]: semaforo INTERRUPCIONES disminuido");
        sem_wait(&sem_interrupciones);

        log_debug(kernel_log, "[INTERRUPT]: esperando mutex_cola_interrupciones");
        pthread_mutex_lock(&mutex_cola_interrupciones);
        log_debug(kernel_log, "[INTERRUPT]: bloqueando mutex_cola_interrupciones");
        t_interrupcion *intr = queue_pop(cola_interrupciones);
        pthread_mutex_unlock(&mutex_cola_interrupciones);

        if (!intr)
        {
            log_error(kernel_log, VERDE("[INTERRUPT]: Interrupt handler: Cola de interrupción vacía"));
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        int fd_interrupt = obtener_fd_interrupt(intr->cpu_a_desalojar->id);
        if (fd_interrupt == -1)
        {
            log_error(kernel_log, VERDE("[INTERRUPT]: Interrupt handler: No se encontró fd_interrupt para CPU %d"), intr->cpu_a_desalojar->id);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }

        log_trace(kernel_log, VERDE("[INTERRUPT]: Interrupt handler: Enviando interrupción a CPU %d (desaloja PID=%d para correr PID=%d)"), intr->cpu_a_desalojar->id, intr->cpu_a_desalojar->pid, intr->pid_a_ejecutar);

        // Enviar interrupción con el PID actualmente ejecutando en la CPU
        t_paquete *paquete = crear_paquete_op(INTERRUPCION_OP);
        agregar_entero_a_paquete(paquete, intr->cpu_a_desalojar->pid);
        enviar_paquete(paquete, fd_interrupt);
        eliminar_paquete(paquete);

        // Esperar respuesta
        int respuesta = recibir_operacion(fd_interrupt);

        switch (respuesta)
        {
        case OK:
            log_trace(kernel_log, VERDE("[INTERRUPT]: CPU %d respondió OK"), intr->cpu_a_desalojar->id);

            t_list *contenido = recibir_contenido_paquete(fd_interrupt);
            if (!contenido)
            {
                log_error(kernel_log, "[INTERRUPT]: El contenido recibido es NULL");
                terminar_kernel();
                exit(EXIT_FAILURE);
            }
            log_trace(kernel_log, "[INTERRUPT]: Cantidad de elementos en contenido recibido: %d", list_size(contenido));

            if (list_size(contenido) < 2)
            {
                log_error(kernel_log, "[INTERRUPT]: Error en buffer recibido de CPU");
                list_destroy_and_destroy_elements(contenido, free);
                terminar_kernel();
                exit(EXIT_FAILURE);
            }

            int pid_recibido = *(int *)list_get(contenido, 0);
            int nuevo_pc = *(int *)list_get(contenido, 1);
            list_destroy_and_destroy_elements(contenido, free);

            if (pid_recibido != intr->cpu_a_desalojar->pid)
            {
                log_error(kernel_log, "[INTERRUPT]: PID recibido (%d) no coincide con PID esperado (%d)", pid_recibido, intr->cpu_a_desalojar->pid);
                terminar_kernel();
                exit(EXIT_FAILURE);
            }

            t_pcb *pcb = buscar_pcb(pid_recibido);

            log_info(kernel_log, MAGENTA("## (%d) - Desalojado por SJF/SRT"), pid_recibido);
            log_trace(kernel_log, "Interrupt handler: Actualizando PCB PID=%d con nuevo PC=%d", pid_recibido, nuevo_pc);

            pcb->PC = nuevo_pc;

            // Limpiar CPU desalojada
            intr->cpu_a_desalojar->pid = -1;
            intr->cpu_a_desalojar->instruccion_actual = -1;     // Resetear instrucción actual
            log_trace(kernel_log, "Interrupt handler: CPU %d liberada", intr->cpu_a_desalojar->id);
            cambiar_estado_pcb(pcb, READY);
            solicitar_replanificacion_srt();
            log_trace(kernel_log, "interrupt_handler: replanificacion solicitada");
            free(intr);
            break;
        case ERROR:
            log_trace(kernel_log, VERDE("[INTERRUPT]: Interrupt handler: CPU %d respondió con ERROR"), intr->cpu_a_desalojar->id);
            free(intr);
            break;
        default:
            log_error(kernel_log, "[INTERRUPT]: Interrupt handler: No se pudo recibir respuesta de CPU %d", intr->cpu_a_desalojar->id);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
    }
}

/*
bool interrupt(cpu* cpu_a_desalojar, t_pcb *proceso_a_ejecutar) {
    log_trace(kernel_log, "Interrupción enviada a CPU %d (fd=%d) para desalojo", cpu_a_desalojar->id, cpu_a_desalojar->fd);
    int fd_interrupt = obtener_fd_interrupt(cpu_a_desalojar->id);

    if (fd_interrupt == -1) {
        log_error(kernel_log, "No se encontró el fd_interrupt para CPU %d", cpu_a_desalojar->id);
        return false;
    }

    // Enviar op code y pid
    t_paquete* paquete = crear_paquete_op(INTERRUPCION_OP);
    enviar_paquete(paquete, fd_interrupt);
    eliminar_paquete(paquete);

    // recibir respuesta
    t_respuesta respuesta;
    if (recv(fd_interrupt, &respuesta, sizeof(t_respuesta), 0) <= 0) {
        log_error(kernel_log, "Error al recibir respuesta de interrupción de CPU %d", cpu_a_desalojar->id);
        terminar_kernel();
        exit(EXIT_FAILURE);
    }

    // Procesar respuesta
    if (respuesta == OK) {
        int buffer_size;
        void* buffer = recibir_buffer(&buffer_size, fd_interrupt);
        if (!buffer) {
            log_error(kernel_log, "Error al recibir buffer de interrupción de CPU %d", cpu_a_desalojar->id);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        // ACtualizar pc del proceso ejecutando, cambiar estado a ready
        int offset = 0;
        if(leer_entero(buffer, &offset) == proceso_a_ejecutar->PID) {
            log_trace(kernel_log, "Interrupción: Proceso %d desalojado de CPU %d", proceso_a_ejecutar->PID, cpu_a_desalojar->id);
            proceso_a_ejecutar->PC = leer_entero(buffer, &offset);
        } else {
            log_error(kernel_log, "Interrupción: PID recibido no coincide con el proceso a desalojar (PID=%d)", proceso_a_ejecutar->PID);
            free(buffer);
            terminar_kernel();
            exit(EXIT_FAILURE);
        }
        free(buffer);
        return true;
    }
    return false;
}*/