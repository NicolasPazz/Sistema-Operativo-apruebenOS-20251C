#include "../headers/mock.h"
#include "../headers/inits.h"

// Mutexes para las colas
pthread_mutex_t mutex_cola_new;
pthread_mutex_t mutex_cola_susp_ready;
pthread_mutex_t mutex_cola_susp_blocked;
pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_running;
pthread_mutex_t mutex_cola_blocked;
pthread_mutex_t mutex_cola_exit;

// Semáforos para sincronización
sem_t sem_proceso_a_new;
sem_t sem_proceso_a_susp_ready;
sem_t sem_proceso_a_susp_blocked;
sem_t sem_proceso_a_ready;
sem_t sem_proceso_a_running;
sem_t sem_proceso_a_blocked;
sem_t sem_proceso_a_exit;
sem_t sem_susp_ready_vacia;
sem_t sem_finalizacion_de_proceso;

// Colas de procesos
t_list* cola_new;
t_list* cola_ready;
t_list* cola_running;
t_list* cola_blocked;
t_list* cola_susp_ready;
t_list* cola_susp_blocked;
t_list* cola_exit;
t_list* cola_procesos;

// Inicializar semáforos y mutexes
void iniciar_sincronizacion_mock() {
    pthread_mutex_init(&mutex_cola_new, NULL);
    pthread_mutex_init(&mutex_cola_susp_ready, NULL);
    pthread_mutex_init(&mutex_cola_susp_blocked, NULL);
    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_running, NULL);
    pthread_mutex_init(&mutex_cola_blocked, NULL);
    pthread_mutex_init(&mutex_cola_exit, NULL);

    sem_init(&sem_proceso_a_new, 0, 0);
    sem_init(&sem_proceso_a_susp_ready, 0, 0);
    sem_init(&sem_proceso_a_susp_blocked, 0, 0);
    sem_init(&sem_proceso_a_ready, 0, 0);
    sem_init(&sem_proceso_a_running, 0, 0);
    sem_init(&sem_proceso_a_blocked, 0, 0);
    sem_init(&sem_proceso_a_exit, 0, 0);
    sem_init(&sem_susp_ready_vacia, 0, 1);
    sem_init(&sem_finalizacion_de_proceso, 0, 0);
}

// Inicializar colas de estados
void iniciar_estados_mock() {
    cola_new = list_create();
    cola_ready = list_create();
    cola_running = list_create();
    cola_blocked = list_create();
    cola_susp_ready = list_create();
    cola_susp_blocked = list_create();
    cola_exit = list_create();
    cola_procesos = list_create();
}

// Liberar recursos
void terminar_mock() {
    pthread_mutex_destroy(&mutex_cola_new);
    pthread_mutex_destroy(&mutex_cola_susp_ready);
    pthread_mutex_destroy(&mutex_cola_susp_blocked);
    pthread_mutex_destroy(&mutex_cola_ready);
    pthread_mutex_destroy(&mutex_cola_running);
    pthread_mutex_destroy(&mutex_cola_blocked);
    pthread_mutex_destroy(&mutex_cola_exit);

    sem_destroy(&sem_proceso_a_new);
    sem_destroy(&sem_proceso_a_susp_ready);
    sem_destroy(&sem_proceso_a_susp_blocked);
    sem_destroy(&sem_proceso_a_ready);
    sem_destroy(&sem_proceso_a_running);
    sem_destroy(&sem_proceso_a_blocked);
    sem_destroy(&sem_proceso_a_exit);
    sem_destroy(&sem_susp_ready_vacia);
    sem_destroy(&sem_finalizacion_de_proceso);

    list_destroy_and_destroy_elements(cola_new, free);
    list_destroy_and_destroy_elements(cola_ready, free);
    list_destroy_and_destroy_elements(cola_running, free);
    list_destroy_and_destroy_elements(cola_blocked, free);
    list_destroy_and_destroy_elements(cola_susp_ready, free);
    list_destroy_and_destroy_elements(cola_susp_blocked, free);
    list_destroy_and_destroy_elements(cola_exit, free);
    list_destroy_and_destroy_elements(cola_procesos, free);
} 