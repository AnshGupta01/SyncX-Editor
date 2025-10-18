//The broadcaster is a client connecting to multiple peer socket servers.

#include "broadcaster.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_PEERS 10
static int peer_fds[MAX_PEERS];
static int num_peers;
static SyncQueue* queue;
static pthread_t broadcaster_thread;
static volatile int running = 0;

static int connect_to_peer(const char* path) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    return sock;
}

bool broadcaster_init(const char** peer_paths, int peer_count, SyncQueue* in_queue) {
    if (peer_count > MAX_PEERS || peer_count <= 0 || !peer_paths || !in_queue)
        return false;

    queue = in_queue;
    num_peers = peer_count;

    for (int i = 0; i < num_peers; i++) {
        peer_fds[i] = connect_to_peer(peer_paths[i]);
        if (peer_fds[i] < 0) {
            perror("connect_to_peer");
        }
    }
    return true;
}

static void send_operation_to_peers(const Operation* op) {
    for (int i = 0; i < num_peers; i++) {
        if (peer_fds[i] < 0) continue;

        ssize_t nsent = send(peer_fds[i], op, sizeof(Operation), 0);
        if (nsent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("send");
            close(peer_fds[i]);
            peer_fds[i] = -1;
        }
    }
}

static void* broadcaster_loop(void* arg) {
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

void broadcaster_start(void) {
    running = 1;
    pthread_create(&broadcaster_thread, NULL, broadcaster_loop, NULL);
}

void broadcaster_stop(void) {
    running = 0;
    pthread_join(broadcaster_thread, NULL);
    for (int i = 0; i < num_peers; i++) {
        if (peer_fds[i] >= 0) close(peer_fds[i]);
    }
}