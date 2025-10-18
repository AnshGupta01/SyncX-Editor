// The receiver is a server accepting multiple client connections.

#include "reciever.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#define MAX_CONNECTION_QUEUE 5

static int server_fd = -1;
static SyncQueue* recv_queue = NULL;
static pthread_t receiver_thread;
static volatile int running = 0;
static char socket_path[108];

static void* receiver_loop(void* arg) {
    while (running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        Operation op;
        ssize_t nread;
        while ((nread = recv(client_fd, &op, sizeof(Operation), 0)) > 0) {
            sync_queue_enqueue(recv_queue, &op);
        }
        close(client_fd);
    }
    return NULL;
}

bool receiver_init(int user_id, SyncQueue* in_queue) {
    snprintf(socket_path, sizeof(socket_path), "/tmp/user_%d.sock", user_id);

    unlink(socket_path);
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return false;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return false;
    }

    if (listen(server_fd, MAX_CONNECTION_QUEUE) < 0) {
        perror("listen");
        close(server_fd);
        return false;
    }

    recv_queue = in_queue;
    return true;
}

void receiver_start(void) {
    running = 1;
    pthread_create(&receiver_thread, NULL, receiver_loop, NULL);
}

void receiver_stop(void) {
    running = 0;
    shutdown(server_fd, SHUT_RDWR);
    pthread_join(receiver_thread, NULL);
    close(server_fd);
    unlink(socket_path);
}