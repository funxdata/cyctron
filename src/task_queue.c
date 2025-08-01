#include "task_queue.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

typedef struct task {
    int client_fd;
    struct task* next;
} task_t;

typedef struct {
    task_t* head;
    task_t* tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} task_queue_t;

static task_queue_t queue;

void task_queue_init(void) {
    queue.head = NULL;
    queue.tail = NULL;
    pthread_mutex_init(&queue.mutex, NULL);
    pthread_cond_init(&queue.cond, NULL);
}

void task_queue_push(int client_fd) {
    task_t* new_task = malloc(sizeof(task_t));
    if (!new_task) {
        perror("malloc");
        return;
    }
    new_task->client_fd = client_fd;
    new_task->next = NULL;

    pthread_mutex_lock(&queue.mutex);
    if (queue.tail) {
        queue.tail->next = new_task;
        queue.tail = new_task;
    } else {
        queue.head = queue.tail = new_task;
    }
    pthread_cond_signal(&queue.cond);
    pthread_mutex_unlock(&queue.mutex);
}

int task_queue_pop(void) {
    pthread_mutex_lock(&queue.mutex);
    while (queue.head == NULL) {
        pthread_cond_wait(&queue.cond, &queue.mutex);
    }
    task_t* task = queue.head;
    int client_fd = task->client_fd;
    queue.head = task->next;
    if (queue.head == NULL) {
        queue.tail = NULL;
    }
    pthread_mutex_unlock(&queue.mutex);

    free(task);
    return client_fd;
}
