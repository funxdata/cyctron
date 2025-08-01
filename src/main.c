#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "server.h"
#include "task_queue.h"

#define SERVER_PORT 44944
#define THREAD_POOL_SIZE 4
#define BUFFER_SIZE 1024

void* worker_thread(void* arg) {
    (void)arg;
    char buffer[BUFFER_SIZE];
    while (1) {
        int client_fd = task_queue_pop();
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Request received from fd %d:\n%s\n", client_fd, buffer);

            const char* response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 13\r\n"
                "\r\n"
                "Hello, world!";

            ssize_t bytes_written = write(client_fd, response, strlen(response));
            if (bytes_written == -1) {
                perror("write");
            }
        } else if (bytes_read == -1) {
            perror("read");
        } else {
            printf("Client fd %d closed connection\n", client_fd);
        }
        close(client_fd);
    }
    return NULL;
}

int main(void) {
    int listen_fd = setup_server_socket(SERVER_PORT);
    if (listen_fd == -1) {
        fprintf(stderr, "Failed to setup server socket\n");
        return EXIT_FAILURE;
    }

    task_queue_init();

    pthread_t threads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0) {
            perror("pthread_create");
            // 这里简单忽略失败
        }
    }

    printf("Server listening on port %d\n", SERVER_PORT);
    if (run_event_loop(listen_fd) == -1) {
        fprintf(stderr, "Error running event loop\n");
        return EXIT_FAILURE;
    }

    // 实际退出时应加入清理线程等代码
    close(listen_fd);
    return EXIT_SUCCESS;
}
