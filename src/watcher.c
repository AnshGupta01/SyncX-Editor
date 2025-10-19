#include "watcher.h"
#include "syncQueue.h"
#include "operation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

static char* filepath = NULL;
static SyncQueue* op_queue = NULL;
static int user_id_global = 0;
static pthread_t watcher_thread;
static volatile bool running = false;

static char** old_lines = NULL;
static size_t old_line_count = 0;

static size_t read_file_lines(const char* path, char*** lines_out) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    char** lines = NULL;
    size_t count = 0;
    char* line = NULL;
    size_t len = 0;
    ssize_t nread;

    while ((nread = getline(&line, &len, f)) != -1) {
        lines = realloc(lines, (count + 1) * sizeof(char*));
        lines[count++] = strdup(line);
    }

    free(line);
    fclose(f);

    *lines_out = lines;
    return count;
}

static void free_lines(char** lines, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
}

static void detect_changes_and_enqueue(char** old, size_t old_count, char** new, size_t new_count) {
    size_t max_count = (old_count > new_count) ? old_count : new_count;

    for (size_t i = 0; i < max_count; i++) {
        const char* old_line = (i < old_count) ? old[i] : "";
        const char* new_line = (i < new_count) ? new[i] : "";

        if (strcmp(old_line, new_line) != 0) {
            Operation op;
            op.user_id = user_id_global;
            op.op_type = 'R';
            op.line_no = (int)i;
            op.col_start = 0;
            op.col_end = 0;
            
            // Use snprintf for safer string copying
            snprintf(op.old_text, sizeof(op.old_text), "%s", old_line);
            snprintf(op.new_text, sizeof(op.new_text), "%s", new_line);
            
            op.timestamp = time(NULL);

            sync_queue_enqueue(op_queue, &op);
        }
    }
}

static void* watcher_loop(void* arg) {
    (void)arg;
    time_t last_mod_time = 0;
    
    while (running) {
        struct stat st;
        if (stat(filepath, &st) == 0 && st.st_mtime != last_mod_time) {
            last_mod_time = st.st_mtime;

            char** new_lines = NULL;
            size_t new_count = read_file_lines(filepath, &new_lines);
            detect_changes_and_enqueue(old_lines, old_line_count, new_lines, new_count);

            free_lines(old_lines, old_line_count);
            old_lines = new_lines;
            old_line_count = new_count;
        }
        usleep(100000);
    }
    return NULL;
}

bool watcher_init(const char* file_path, SyncQueue* queue, int user_id) {
    filepath = strdup(file_path);
    op_queue = queue;
    user_id_global = user_id;

    old_lines = NULL;
    old_line_count = read_file_lines(filepath, &old_lines);

    return true;
}

void watcher_start(int interval_ms) {
    (void)interval_ms;
    running = true;
    pthread_create(&watcher_thread, NULL, watcher_loop, NULL);
}

void watcher_stop(void) {
    running = false;
    pthread_join(watcher_thread, NULL);
    free(filepath);
    free_lines(old_lines, old_line_count);
}