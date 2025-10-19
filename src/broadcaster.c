#include "broadcaster.h"
#include "syncQueue.h"
#include "operation.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#define MAX_PEERS 10

static int peer_fds[MAX_PEERS];
static int num_peers = 0;
static SyncQueue* queue = NULL;
static pthread_t broadcaster_thread;
static volatile int running = 0;

static int connect_to_peer(const char* socket_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void send_operation_to_peers(const Operation* op) {
    for (int i = 0; i < num_peers; i++) {
        if (peer_fds[i] >= 0) {
            ssize_t sent = send(peer_fds[i], op, sizeof(Operation), 0);
            if (sent < 0) {
                perror("send to peer");
            }
        }
    }
}

static void* broadcaster_loop(void* arg) {
    (void)arg;
    
    Operation op;
    while (running) {
        if (sync_queue_dequeue(queue, &op)) {
            send_operation_to_peers(&op);
        } else {
            usleep(1000);
        }
    }
    return NULL;
}

bool broadcaster_init(const char* peer_queues_in[], int num_peers_in, SyncQueue* in_queue) {
    num_peers = 0;
    queue = in_queue;

    for (int i = 0; i < num_peers_in && i < MAX_PEERS; i++) {
        peer_fds[num_peers] = connect_to_peer(peer_queues_in[i]);
        if (peer_fds[num_peers] >= 0) {
            num_peers++;
        } else {
            fprintf(stderr, "Failed to connect to peer: %s\n", peer_queues_in[i]);
        }
    }

    return true;
}

void broadcaster_start(void) {
    running = 1;
    pthread_create(&broadcaster_thread, NULL, broadcaster_loop, NULL);
}

void broadcaster_stop(void) {
    running = 0;
    pthread_join(broadcaster_thread, NULL);

    for (int i = 0; i < num_peers; i++) {
        if (peer_fds[i] >= 0) {
            close(peer_fds[i]);
            peer_fds[i] = -1;
        }
    }
}