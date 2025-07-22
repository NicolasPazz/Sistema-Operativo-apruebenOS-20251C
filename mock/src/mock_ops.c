#include "../headers/mock.h"
#include "../headers/inits.h"
#include "../headers/estructuras.h"
#include "../../utils/headers/sockets.h"
#include "../../utils/headers/serializacion.h"
#include "../../utils/headers/utils.h"
#include <commons/log.h>
#include <commons/string.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// Variables globales
int fd_memoria;
extern t_log* mock_log;

// Variable global para el siguiente PID
static int siguiente_pid = 1;

// Función para obtener el siguiente PID disponible
static int obtener_siguiente_pid() {
    return siguiente_pid++;
}

/////////////////////////////// Kernel -> Memoria ///////////////////////////////

void INIT_PROC(char* nombre_archivo, int tam_memoria) {
    log_trace(mock_log, "## Solicitó syscall: INIT_PROC para archivo %s con tamaño %d", nombre_archivo, tam_memoria);
    
    // Comunicarse con memoria para inicializar proceso
    t_paquete* paquete = crear_paquete_op(INIT_PROC_OP);
    
    // Agregar campos en el orden que espera memoria: PID, nombre, tamaño
    int pid = obtener_siguiente_pid();
    agregar_a_paquete(paquete, &pid, sizeof(int));
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);
    agregar_a_paquete(paquete, &tam_memoria, sizeof(int));
    
    log_trace(mock_log, "Enviando paquete INIT_PROC_OP con: PID=%d, nombre=%s, tamaño=%d", pid, nombre_archivo, tam_memoria);
    
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
    
    // Esperar respuesta de memoria (t_respuesta)
    t_respuesta respuesta;
    if (recv(fd_memoria, &respuesta, sizeof(t_respuesta), 0) <= 0) {
        log_trace(mock_log, "Error al recibir respuesta de memoria para INIT_PROC");
        return;
    }
    
    if (respuesta == OK) {
        // Crear nuevo proceso
        t_pcb* nuevo_proceso = malloc(sizeof(t_pcb));
        nuevo_proceso->PID = pid;  // Usar el mismo PID que enviamos a memoria
        nuevo_proceso->Estado = NEW;
        nuevo_proceso->tamanio_memoria = tam_memoria;
        nuevo_proceso->path = strdup(nombre_archivo);
        
        // Agregar a cola_new
        pthread_mutex_lock(&mutex_cola_new);
        list_add(cola_new, nuevo_proceso);
        pthread_mutex_unlock(&mutex_cola_new);
        
        // Señalar que hay un nuevo proceso
        sem_post(&sem_proceso_a_new);
        
        log_trace(mock_log, "## (%d) - Proceso creado", nuevo_proceso->PID);
    } else {
        log_trace(mock_log, "Error al inicializar proceso en memoria");
    }
}

void FINALIZAR_PROC(int pid) {
    log_trace(mock_log, "## Solicitó syscall: FINALIZAR_PROC para PID %d", pid);
    
    // Comunicarse con memoria para finalizar proceso
    t_paquete* paquete = crear_paquete_op(FINALIZAR_PROC_OP);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
    
    // Esperar respuesta de memoria
    t_list* lista_respuesta = recibir_paquete(fd_memoria);
    if (lista_respuesta == NULL) {
        log_trace(mock_log, "Error al recibir respuesta de memoria para FINALIZAR_PROC");
        return;
    }
    
    // Procesar respuesta
    t_paquete* respuesta = list_get(lista_respuesta, 0);
    if (respuesta->codigo_operacion == FINALIZAR_PROC_OP) {
        // Buscar y eliminar el proceso de todas las colas
        t_pcb* proceso = NULL;
        
        // Buscar en cola_new
        pthread_mutex_lock(&mutex_cola_new);
        for (int i = 0; i < list_size(cola_new); i++) {
            t_pcb* p = list_get(cola_new, i);
            if (p->PID == pid) {
                proceso = list_remove(cola_new, i);
                break;
            }
        }
        pthread_mutex_unlock(&mutex_cola_new);
        
        // Si no está en NEW, buscar en otras colas...
        if (proceso == NULL) {
            // TODO: Implementar búsqueda en otras colas
        }
        
        if (proceso != NULL) {
            free(proceso->path);
            free(proceso);
            log_info(mock_log, "## (%d) - Finaliza el proceso", pid);
        }
    } else {
        log_trace(mock_log, "Error al finalizar proceso en memoria");
    }
    
    list_destroy_and_destroy_elements(lista_respuesta, (void*)eliminar_paquete);
}

void DUMP_MEMORY(int pid) {
    log_trace(mock_log, "## Solicitó syscall: DUMP_MEMORY para PID %d", pid);
    
    // Comunicarse con memoria para hacer dump
    t_paquete* paquete = crear_paquete_op(DUMP_MEMORY_OP);
    agregar_a_paquete(paquete, &pid, sizeof(int));
    
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
    
    // Esperar respuesta de memoria
    t_list* lista_respuesta = recibir_paquete(fd_memoria);
    if (lista_respuesta == NULL) {
        log_trace(mock_log, "Error al recibir respuesta de memoria para DUMP_MEMORY");
        return;
    }
    
    // Procesar respuesta
    t_paquete* respuesta = list_get(lista_respuesta, 0);
    if (respuesta->codigo_operacion == DUMP_MEMORY_OP) {
        log_trace(mock_log, "## (%d) - Memory Dump completado", pid);
    } else {
        log_trace(mock_log, "Error al realizar memory dump");
    }
    
    list_destroy_and_destroy_elements(lista_respuesta, (void*)eliminar_paquete);
}

bool CHECK_MEMORY_SPACE(int tamanio) {
    log_trace(mock_log, "## Solicitó syscall: CHECK_MEMORY_SPACE para tamaño %d", tamanio);
    
    // Comunicarse con memoria para verificar espacio
    t_paquete* paquete = crear_paquete_op(CHECK_MEMORY_SPACE_OP);
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    
    enviar_paquete(paquete, fd_memoria);
    eliminar_paquete(paquete);
    
    // Esperar respuesta de memoria
    t_list* lista_respuesta = recibir_paquete(fd_memoria);
    if (lista_respuesta == NULL) {
        log_trace(mock_log, "Error al recibir respuesta de memoria para CHECK_MEMORY_SPACE");
        return false;
    }
    
    // Procesar respuesta
    t_paquete* respuesta = list_get(lista_respuesta, 0);
    bool hay_espacio = respuesta->codigo_operacion == CHECK_MEMORY_SPACE_OP;
    
    list_destroy_and_destroy_elements(lista_respuesta, (void*)eliminar_paquete);
    
    return hay_espacio;
}

/////////////////////////////// CPU -> Memoria ///////////////////////////////

bool PEDIR_INSTRUCCION_OP_mock(int cliente_socket) { 
    int pid;
    int pc;

    printf("PID del proceso: ");
    scanf("%d", &pid);
    if (!send_data(cliente_socket, &pid, sizeof(pid))) return false;

    printf("PC del proceso: ");
    scanf("%d", &pc);
    if (!send_data(cliente_socket, &pc, sizeof(pc))) return false;

    // Recibo la instrucción desde Memoria
    t_list* lista_respuesta = recibir_paquete(cliente_socket);
    if (lista_respuesta == NULL) {
        log_trace(mock_log, "Error al recibir instrucción de memoria");
        return false;
    }

    // Procesar la instrucción recibida
    t_paquete* respuesta = list_get(lista_respuesta, 0);
    if (respuesta->codigo_operacion == PEDIR_INSTRUCCION_OP) {
        // Extraer los parámetros de la instrucción
        void* stream = respuesta->buffer->stream;
        int offset = 0;

        // Leer cada parámetro
        for (int i = 0; i < 3; i++) {
            int size;
            memcpy(&size, stream + offset, sizeof(int));
            offset += sizeof(int);

            char* param = malloc(size);
            memcpy(param, stream + offset, size);
            offset += size;

            log_trace(mock_log, "Param %d: [%s]", i, param);
            free(param);
        }
    }

    list_destroy_and_destroy_elements(lista_respuesta, (void*)eliminar_paquete);
    return true;
}

// Mocks de operaciones no implementadas
bool PEDIR_CONFIG_CPU_OP_mock() { return true; }
bool IO_FINALIZADA_OP_mock() { return true; }
bool DEBUGGER_mock() { return true; }
bool NOOP_OP_mock() { return true; }
bool WRITE_OP_mock() { return true; }
bool READ_OP_mock() { return true; }
bool GOTO_OP_mock() { return true; }
bool PEDIR_PAGINA_OP_mock() { return true; }

/////////////////////////////// Funciones Mock Faltantes ///////////////////////////////

void MENSAJE_OP_mock() {
    log_trace(mock_log, "## Ejecutando MENSAJE_OP_mock");
}

void PAQUETE_OP_mock() {
    log_trace(mock_log, "## Ejecutando PAQUETE_OP_mock");
}

void IO_OP_mock() {
    log_trace(mock_log, "## Ejecutando IO_OP_mock");
}

void INIT_PROC_OP_mock(int fd_a_testear) {
    log_trace(mock_log, "## Ejecutando INIT_PROC_OP_mock");
    char nombre_archivo[100];
    int tamanio;
    
    printf("Nombre del archivo: ");
    scanf("%s", nombre_archivo);
    printf("Tamaño del proceso: ");
    scanf("%d", &tamanio);
    
    INIT_PROC(nombre_archivo, tamanio);
}

void DUMP_MEMORY_OP_mock() {
    log_trace(mock_log, "## Ejecutando DUMP_MEMORY_OP_mock");
    int pid;
    printf("PID del proceso: ");
    scanf("%d", &pid);
    DUMP_MEMORY(pid);
}

void EXIT_OP_mock() {
    log_trace(mock_log, "## Ejecutando EXIT_OP_mock");
}

void EXEC_OP_mock() {
    log_trace(mock_log, "## Ejecutando EXEC_OP_mock");
}

void INTERRUPCION_OP_mock() {
    log_trace(mock_log, "## Ejecutando INTERRUPCION_OP_mock");
}

void FINALIZAR_PROC_OP_mock() {
    log_trace(mock_log, "## Ejecutando FINALIZAR_PROC_OP_mock");
    int pid;
    printf("PID del proceso: ");
    scanf("%d", &pid);
    FINALIZAR_PROC(pid);
}