#ifndef KERNEL_H
#define KERNEL_H

#include "../../utils/headers/sockets.h"
#include "procesos.h"
#include "syscalls.h"
#include "planificadores.h"
#include "IOKernel.h"
#include "CPUKernel.h"
#include "MEMKernel.h"
#include "organizador_eventos.h"
#include <signal.h>
#include <unistd.h>
#include <commons/collections/queue.h>

// Logger
extern t_log *kernel_log;

// Cronometro para MT en PCB
extern t_dictionary *tiempos_por_pid;
extern t_dictionary *archivo_por_pcb;

// Sockets
extern int fd_kernel_dispatch;
extern int fd_interrupt;

extern bool kernel_finalizado;

// Config
extern t_config *kernel_config;
extern char *IP_MEMORIA;
extern char *PUERTO_MEMORIA;
extern char *PUERTO_ESCUCHA_DISPATCH;
extern char *PUERTO_ESCUCHA_INTERRUPT;
extern char *PUERTO_ESCUCHA_IO;
extern char *ALGORITMO_CORTO_PLAZO;
extern char *ALGORITMO_INGRESO_A_READY;
extern double ALFA;
extern double TIEMPO_SUSPENSION;
extern double ESTIMACION_INICIAL;
extern char *LOG_LEVEL;

// Colas de Estados
extern t_list *cola_new;
extern t_list *cola_ready;
extern t_list *cola_running;
extern t_list *cola_blocked;
extern t_list *cola_susp_ready;
extern t_list *cola_susp_blocked;
extern t_list *cola_exit;
extern t_list *cola_procesos;
extern t_list *pcbs_bloqueados_por_dump_memory;
extern t_list *pcbs_esperando_io;
extern t_queue *cola_interrupciones;

// Listas y semaforos de CPUs y IOs conectadas
extern t_list *lista_cpus;
extern t_list *lista_ios;

// Conexiones
extern pthread_mutex_t mutex_lista_cpus;
extern pthread_mutex_t mutex_ios;
extern t_list* lista_hilos;
extern t_list* lista_sockets;
extern pthread_mutex_t mutex_hilos;
extern pthread_mutex_t mutex_sockets;


// Conexiones minimas
extern bool conectado_cpu;
extern bool conectado_io;
extern pthread_mutex_t mutex_conexiones;

#define SEM_WAIT(sem)                                                                     \
    do                                                                                    \
    {                                                                                     \
        log_trace(kernel_log, ROJO("SEM_WAIT(%s): disminuido en %s..."), #sem, __func__); \
        sem_wait(&(sem));                                                                 \
        log_trace(kernel_log, AMARILLO("SEM_WAIT(%s): recibido en %s."), #sem, __func__); \
    } while (0)

#define SEM_POST(sem)                                                                   \
    do                                                                                  \
    {                                                                                   \
        sem_post(&(sem));                                                               \
        log_trace(kernel_log, VERDE("SEM_POST(%s): aumentado en %s."), #sem, __func__); \
    } while (0)

#define LOCK_CON_LOG(mutex)                                                                      \
    do                                                                                           \
    {                                                                                            \
        log_trace(kernel_log, ROJO("LOCK_CON_LOG(%s): esperando en %s..."), #mutex, __func__);   \
        pthread_mutex_lock(&(mutex));                                                            \
        log_trace(kernel_log, AMARILLO("LOCK_CON_LOG(%s): bloqueado en %s."), #mutex, __func__); \
    } while (0)

#define UNLOCK_CON_LOG(mutex)                                                                  \
    do                                                                                         \
    {                                                                                          \
        pthread_mutex_unlock(&(mutex));                                                        \
        log_trace(kernel_log, VERDE("UNLOCK_CON_LOG(%s): liberado en %s."), #mutex, __func__); \
    } while (0)

#define LOCK_CON_LOG_PCB(mutex, pid)                                                                       \
    do                                                                                                     \
    {                                                                                                      \
        log_trace(kernel_log, ROJO("LOCK_CON_LOG(%s) (%d): esperando en %s..."), #mutex, pid, __func__);   \
        pthread_mutex_lock(&(mutex));                                                                      \
        log_trace(kernel_log, AMARILLO("LOCK_CON_LOG(%s) (%d): bloqueado en %s."), #mutex, pid, __func__); \
    } while (0)

#define UNLOCK_CON_LOG_PCB(mutex, pid)                                                                   \
    do                                                                                                   \
    {                                                                                                    \
        pthread_mutex_unlock(&(mutex));                                                                  \
        log_trace(kernel_log, VERDE("UNLOCK_CON_LOG(%s) (%d): liberado en %s."), #mutex, pid, __func__); \
    } while (0)

#define LOG_DEBUG(logger, fmt, ...)                             \
    do                                                          \
    {                                                           \
        log_debug(logger, "%s: " fmt, __func__, ##__VA_ARGS__); \
    } while (0)

#define LOG_WARNING(logger, fmt, ...)                             \
    do                                                            \
    {                                                             \
        log_warning(logger, "%s: " fmt, __func__, ##__VA_ARGS__); \
    } while (0)

#define LOG_ERROR(logger, fmt, ...)                             \
    do                                                          \
    {                                                           \
        log_error(logger, "%s: " fmt, __func__, ##__VA_ARGS__); \
    } while (0)

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                       INICIALIZACIONES                                       //
//////////////////////////////////////////////////////////////////////////////////////////////////

void iniciar_config_kernel();
void iniciar_logger_kernel();
void iniciar_logger_kernel_debug();
void iniciar_estados_kernel();
void iniciar_sincronizacion_kernel();
void iniciar_diccionario_tiempos();
void iniciar_diccionario_archivos_por_pcb();
void terminar_kernel(int code);
void terminar_hilo();

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                            MEMORIA                                           //
//////////////////////////////////////////////////////////////////////////////////////////////////

void *hilo_cliente_memoria(void *_);

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                         CPU DISPATCH                                         //
//////////////////////////////////////////////////////////////////////////////////////////////////

void *hilo_servidor_dispatch(void *_);
void *atender_cpu_dispatch(void *arg);

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                         CPU INTERRUPT                                        //
//////////////////////////////////////////////////////////////////////////////////////////////////

void *hilo_servidor_interrupt(void *_);

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                              IO                                              //
//////////////////////////////////////////////////////////////////////////////////////////////////

void *hilo_servidor_io(void *_);
void *atender_io(void *arg);

#endif /* KERNEL_H */
