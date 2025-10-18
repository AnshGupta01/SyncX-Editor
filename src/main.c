#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "syncQueue.h"
#include "watcher.h"
#include "broadcaster.h"
#include "reciever.h"
#include "crdt.h"
#include "utils.h"

static volatile int running = 1;

static void handle_signal(int sig) {
    running = 0;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <user_id> <watched_file> <peer_socket1> [peer_socket2 ...]\n", argv[0]);
        return 1;
    }

    int user_id = atoi(argv[1]);
    const char* watched_file = argv[2];
    int peer_count = argc - 3;
    const char** peer_sockets = (const char**)&argv[3];

    // Setup termination signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    SyncQueue* op_queue = sync_queue_create();
    if (!op_queue) {
        fprintf(stderr, "Failed to create operation queue.\n");
        return 1;
    }

    crdt_init();

    if (!watcher_init(watched_file, op_queue, user_id)) {
        fprintf(stderr, "Watcher init failed.\n");
        return 1;
    }

    if (!broadcaster_init_unix(peer_sockets, peer_count, op_queue)) {
        fprintf(stderr, "Broadcaster init failed.\n");
        return 1;
    }

    if (!receiver_init_unix(user_id, op_queue)) {
        fprintf(stderr, "Receiver init failed.\n");
        return 1;
    }

    watcher_start(100);
    broadcaster_start_unix();
    receiver_start_unix();

    printf("SyncText collaborative editor started for user %d\n", user_id);

    // Main loop waits for termination signal
    while (running) {
        // Can add status or periodic sync logic here
        sleep(1);
    }

    printf("\nShutting down...\n");

    watcher_stop();
    broadcaster_stop_unix();
    receiver_stop_unix();

    crdt_destroy();
    sync_queue_destroy(op_queue);

    return 0;
}