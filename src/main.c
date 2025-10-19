#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "syncQueue.h"
#include "watcher.h"
#include "broadcaster.h"
#include "reciever.h"
#include "crdt.h"
#include "utils.h"
#include "registry.h"

#define LOCAL_DOCS_DIR "local_docs"
#define TEMPLATE_FILE "template.txt"

// Global variables for shared access
char local_doc_path[512];
static volatile sig_atomic_t running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

void build_local_doc_path(int user_id, char* buf, size_t bufsize) {
    snprintf(buf, bufsize, "%s/%d_doc.txt", LOCAL_DOCS_DIR, user_id);
}

void build_socket_path(int user_id, char* buf, size_t bufsize) {
    snprintf(buf, bufsize, "/tmp/user_%d.sock", user_id);
}

int file_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

void ensure_directory(const char* dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        mkdir(dir, 0700);
    }
}

void initialize_local_copy(const char* local_path, const char* template_path) {
    if (!file_exists(local_path)) {
        FILE* src = fopen(template_path, "r");
        FILE* dest = fopen(local_path, "w");

        if (!dest) {
            fprintf(stderr, "Failed to create local doc %s\n", local_path);
            return;
        }

        if (src) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
                fwrite(buf, 1, n, dest);
            }
            fclose(src);
        } else {
            fprintf(dest, "Welcome to SyncText!\n");
        }

        fclose(dest);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <user_id>\n", argv[0]);
        return 1;
    }

    int user_id = atoi(argv[1]);
    if (user_id < 1 || user_id > MAX_USERS) {
        fprintf(stderr, "User ID must be between 1 and %d\n", MAX_USERS);
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    ensure_directory(LOCAL_DOCS_DIR);

    char socket_path[108];
    build_local_doc_path(user_id, local_doc_path, sizeof(local_doc_path));
    build_socket_path(user_id, socket_path, sizeof(socket_path));

    initialize_local_copy(local_doc_path, TEMPLATE_FILE);

    if (!registry_init()) {
        fprintf(stderr, "Failed to initialize registry\n");
        return 1;
    }

    if (!registry_register_user(user_id, socket_path)) {
        fprintf(stderr, "Failed to register user\n");
        registry_cleanup();
        return 1;
    }

    SyncQueue* op_queue = sync_queue_create();
    if (!op_queue) {
        fprintf(stderr, "Failed to create operation queue\n");
        registry_unregister_user(user_id);
        registry_cleanup();
        return 1;
    }

    crdt_init();

    if (!watcher_init(local_doc_path, op_queue, user_id)) {
        fprintf(stderr, "Watcher init failed\n");
        crdt_destroy();
        sync_queue_destroy(op_queue);
        registry_unregister_user(user_id);
        registry_cleanup();
        return 1;
    }

    char peer_sockets[MAX_USERS][108];
    int peer_count = registry_get_active_users(peer_sockets, MAX_USERS, user_id);
    
    const char* peer_paths[MAX_USERS];
    for (int i = 0; i < peer_count; i++) {
        peer_paths[i] = peer_sockets[i];
    }

    if (!broadcaster_init(peer_paths, peer_count, op_queue)) {
        fprintf(stderr, "Broadcaster init failed (no peers available yet)\n");
    }

    if (!receiver_init(user_id, op_queue)) {
        fprintf(stderr, "Receiver init failed\n");
        watcher_stop();
        broadcaster_stop();
        crdt_destroy();
        sync_queue_destroy(op_queue);
        registry_unregister_user(user_id);
        registry_cleanup();
        return 1;
    }

    watcher_start(100);
    broadcaster_start();
    receiver_start();

    printf("SyncText started for user %d\n", user_id);
    printf("Watching: %s\n", local_doc_path);
    printf("Socket: %s\n", socket_path);
    printf("Connected peers: %d\n\n", peer_count);

    while (running) {
        sleep(1);
    }

    printf("\nShutting down...\n");

    watcher_stop();
    broadcaster_stop();
    receiver_stop();

    crdt_destroy();
    sync_queue_destroy(op_queue);

    registry_unregister_user(user_id);
    registry_cleanup();

    if (remove(local_doc_path) == 0) {
        printf("Deleted local document: %s\n", local_doc_path);
    }

    printf("Cleanup complete\n");

    return 0;
}