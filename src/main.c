#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
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
    cleanup_local_docs();
}

static void refresh_peer_list_and_display(int include_self) {
    int active_ids[MAX_USERS];
    char active_queues[MAX_USERS][64];
    
    int active_count = registry_get_active_users(active_ids, active_queues, MAX_USERS, current_user_id);
    
    if (include_self) {
        active_ids[active_count++] = current_user_id;
    }
    
    crdt_lock();
    utils_display_document(local_doc_path, current_user_id, crdt_get_document_lines(),
                           crdt_get_line_count(), active_ids, active_count, last_change_msg);
    crdt_unlock();
    
    // Update broadcaster peers
    active_count = registry_get_active_users(active_ids, active_queues, MAX_USERS, current_user_id);
    broadcaster_update_peers(active_ids, active_queues, active_count);
}

static void on_local_change(const Operation* op) {
    if (!op) return;
    
    snprintf(last_change_msg, sizeof(last_change_msg),
             "Change detected: Line %d modified by user_%d (timestamp: %ld)",
             op->line_no, op->user_id, op->timestamp);
    
    broadcaster_add_operation(op);
    /* Before + after: if the watcher runs mid-save, the trailing note
     * still forces a baseline refresh after the file is complete. */
    watcher_note_self_write();
    crdt_merge_and_save(op, local_doc_path);
    watcher_note_self_write();
    
    refresh_peer_list_and_display(1);
}

static void on_remote_operation(const Operation* op) {
    if (!op) return;
    
    snprintf(last_change_msg, sizeof(last_change_msg),
             "Received update from user_%d: Line %d modified (timestamp: %ld)",
             op->user_id, op->line_no, op->timestamp);
    
    watcher_note_self_write();
    crdt_merge_and_save(op, local_doc_path);
    watcher_note_self_write();
    
    refresh_peer_list_and_display(1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <user_id>\n", argv[0]);
        fprintf(stderr, "Example: %s 1\n", argv[0]);
        return 1;
    }
    
    current_user_id = atoi(argv[1]);
    
    if (current_user_id < 1 || current_user_id > 99) {
        fprintf(stderr, "Error: user_id must be between 1 and 99\n");
        return 1;
    }
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Create local docs directory
    if (mkdir(LOCAL_DOCS_DIR, 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
        // Continue anyway
    }
    
    snprintf(local_doc_path, sizeof(local_doc_path), "%s/user_%d_doc.txt", 
             LOCAL_DOCS_DIR, current_user_id);
    
    // Create initial document if it doesn't exist
    FILE* f = fopen(local_doc_path, "a+");
    if (f) {
        fseek(f, 0, SEEK_END);
        if (ftell(f) == 0) {
            // Open template file
            FILE* tf = fopen("include/template.txt", "r");
            if (tf) {
                char buf[4096];
                while (fgets(buf, sizeof(buf), tf)) {
                    fputs(buf, f);
                }
                fclose(tf);
            } else {
                // Fallback: basic default text if template missing
                fprintf(f, "Welcome to SyncText\n");
                fprintf(f, "This is the default template text, if not given any template\n");
                fprintf(f, "Edit this document and see real-time updates\n");
            }
        }
        fclose(f);
    }
    
    // Initialize registry
    if (!registry_init()) {
        fprintf(stderr, "Failed to initialize registry\n");
        return 1;
    }
    
    // Register this user
    char queue_name[64];
    snprintf(queue_name, sizeof(queue_name), "/queue_user_%d", current_user_id);
    
    if (!registry_register_user(current_user_id, queue_name)) {
        fprintf(stderr, "Failed to register user\n");
        registry_cleanup();
        return 1;
    }
    
    // Initialize CRDT
    crdt_init();
    crdt_load_from_file(local_doc_path);
    
    // Initialize components
    if (!watcher_init(local_doc_path, current_user_id, on_local_change)) {
        fprintf(stderr, "Failed to initialize watcher\n");
        crdt_destroy();
        registry_unregister_user(current_user_id);
        registry_cleanup();
        return 1;
    }
    
    if (!broadcaster_init()) {
        fprintf(stderr, "Failed to initialize broadcaster\n");
        crdt_destroy();
        registry_unregister_user(current_user_id);
        registry_cleanup();
        return 1;
    }
    
    if (!receiver_init(current_user_id, on_remote_operation)) {
        fprintf(stderr, "Failed to initialize receiver\n");
        crdt_destroy();
        registry_unregister_user(current_user_id);
        registry_cleanup();
        return 1;
    }
    
    // Start threads
    watcher_start();
    broadcaster_start();
    receiver_start();
    
    printf("\n");
    printf("========================================\n");
    printf("  SyncText Started Successfully!\n");
    printf("========================================\n");
    printf("Registered as user_%d\n", current_user_id);
    printf("Message queue created: %s\n", queue_name);
    printf("Local document: %s\n", local_doc_path);
    printf("========================================\n\n");
    
    // Initial display
    refresh_peer_list_and_display(1);
    
    // Main loop
    while (running) {
        sleep(2);
        refresh_peer_list_and_display(1);  // Periodically refresh peer list
    }
    
    printf("\n\nShutting down gracefully...\n");
    
    // Stop threads
    watcher_stop();
    broadcaster_stop();
    receiver_stop();
    
    // Cleanup
    crdt_destroy();
    registry_unregister_user(current_user_id);
    registry_cleanup();
    cleanup_local_docs();
    
    printf("Cleanup complete. Goodbye!\n");
    
    return 0;
}