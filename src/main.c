#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "registry.h"
#include "crdt.h"
#include "watcher.h"
#include "broadcaster.h"
#include "receiver.h"
#include "utils.h"

#define LOCAL_DOCS_DIR "local_docs"

static volatile sig_atomic_t running = 1;
static int current_user_id = 0;
static char local_doc_path[256];
static char last_change_msg[512] = "No changes yet";

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static void refresh_peer_list_and_display(int include_self) {
    int active_ids[MAX_USERS];
    char active_queues[MAX_USERS][64];
    int active_count = registry_get_active_users(active_ids, active_queues, MAX_USERS, current_user_id);
    if (include_self) active_ids[active_count++] = current_user_id; // Include self last

    utils_display_document(local_doc_path, current_user_id, crdt_get_document_lines(), 
                          crdt_get_line_count(), active_ids, active_count, last_change_msg);

    // Also update broadcaster peers
    active_count = registry_get_active_users(active_ids, active_queues, MAX_USERS, current_user_id);
    broadcaster_update_peers(active_ids, active_queues, active_count);

    printf("[DEBUG] Found %d peers:\n", active_count);
    for (int i = 0; i < active_count; i++) {
        printf("  - user_%d @ %s\n", active_ids[i], active_queues[i]);
    }
}

static void on_local_change(const Operation* op) {
    snprintf(last_change_msg, sizeof(last_change_msg), 
             "Change detected: Line %d modified by user_%d", op->line_no, op->user_id);
    broadcaster_add_operation(op);
    crdt_merge_operation(op);
    crdt_save_to_file(local_doc_path);
    refresh_peer_list_and_display(1);
}

static void on_remote_operation(const Operation* op) {
    snprintf(last_change_msg, sizeof(last_change_msg), 
             "Received update from user_%d: Line %d modified", op->user_id, op->line_no);
    crdt_merge_operation(op);
    crdt_save_to_file(local_doc_path);
    refresh_peer_list_and_display(1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <user_id>\n", argv[0]);
        return 1;
    }

    current_user_id = atoi(argv[1]);
    signal(SIGINT, handle_signal);

    mkdir(LOCAL_DOCS_DIR, 0755);
    snprintf(local_doc_path, sizeof(local_doc_path), "%s/user_%d_doc.txt", LOCAL_DOCS_DIR, current_user_id);

    // Create initial document if not exists
    FILE* f = fopen(local_doc_path, "a+");
    if (f) {
        fseek(f, 0, SEEK_END);
        if (ftell(f) == 0) {
            fprintf(f, "Line 0: Hello World\n");
            fprintf(f, "Line 1: This is a collaborative editor\n");
            fprintf(f, "Line 2: Welcome to SyncText\n");
            fprintf(f, "Line 3: Edit this document and see real-time updates\n");
        }
        fclose(f);
    }

    if (!registry_init()) {
        fprintf(stderr, "Failed to initialize registry\n");
        return 1;
    }

    char queue_name[64];
    snprintf(queue_name, sizeof(queue_name), "/queue_user_%d", current_user_id);

    if (!registry_register_user(current_user_id, queue_name)) {
        fprintf(stderr, "Failed to register user\n");
        registry_cleanup();
        return 1;
    }

    crdt_init();
    crdt_load_from_file(local_doc_path);

    watcher_init(local_doc_path, current_user_id, on_local_change);
    broadcaster_init();
    receiver_init(current_user_id, on_remote_operation);

    // Print peer info and update to broadcaster
    refresh_peer_list_and_display(1);

    watcher_start();
    broadcaster_start();
    receiver_start();

    printf("Registered as user_%d\n", current_user_id);
    printf("Message queue created: %s\n", queue_name);

    // Initial display
    refresh_peer_list_and_display(1);

    while (running) {
        sleep(2);
        refresh_peer_list_and_display(1);  // Periodically refresh peer list for new users
    }

    printf("\nShutting down...\n");

    watcher_stop();
    broadcaster_stop();
    receiver_stop();
    crdt_destroy();
    registry_unregister_user(current_user_id);
    registry_cleanup();

    printf("Cleanup complete\n");
    return 0;
}