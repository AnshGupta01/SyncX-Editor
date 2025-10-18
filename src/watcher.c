// Handles file monitoring (using stat()), detects changed lines, computes differences, 
// and generates Operation objects.

#include "watcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

static char** old_lines = NULL;     // Cached file lines before last change
static size_t old_line_count = 0;
static char filepath[512];
static SyncQueue* queue = NULL;
static int watcher_user_id = 0;
static pthread_t watcher_thread;
static volatile bool running = false;

// Reads file lines into dynamically allocated array; returns count
// lines_out is a pointer to an array of strings as arrayOfStr is char**
static size_t read_file_lines(const char* path, char*** lines_out) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    char** lines = NULL;
    size_t capacity = 0, count = 0;
    char buffer[512];

    while (fgets(buffer, sizeof(buffer), f)) {
        if (count >= capacity) {
            //1st time - makes 16, then doubles each time
            capacity = capacity ? capacity * 2 : 16;
            lines = realloc(lines, capacity * sizeof(char*));
        }
        lines[count] = strdup(buffer);
        count++;
    }
    fclose(f);
    *lines_out = lines;
    return count;
}

// Frees previously allocated lines
static void free_lines(char** lines, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
}

// Compares old vs new lines, detects differences, creates operations
static void detect_changes_and_enqueue(char** old_lines, size_t old_count, char** new_lines, size_t new_count) {
    size_t max_lines = old_count > new_count ? old_count : new_count;

    for (size_t i = 0; i < max_lines; i++) {
        const char* old_line = i < old_count ? old_lines[i] : "";
        const char* new_line = i < new_count ? new_lines[i] : "";

        if (strcmp(old_line, new_line) != 0) {
            // For simplicity, create a single replace operation per line difference
            Operation op;
            op.op_type = 'R';
            op.line_no = (int)i;
            op.col_start = 0;
            op.col_end = (int)strlen(old_line);
            strncpy(op.new_text, new_line, sizeof(op.new_text) - 1);
            op.new_text[sizeof(op.new_text) - 1] = '\0';
            op.timestamp = time(NULL);
            op.user_id = watcher_user_id;

            // Enqueue operation; drop it if queue is full (optionally retry)
            sync_queue_enqueue(queue, &op);
        }
    }
}

// Thread loop for monitoring file changes
static void* watcher_loop(void* arg) {
    time_t last_mod_time = 0;

    while (running) {
        struct stat st;
        // stat() checks if file exista and if it can be retrieved
        if (stat(filepath, &st) == 0 && st.st_mtime != last_mod_time) {
            last_mod_time = st.st_mtime;

            char** new_lines = NULL;
            size_t new_count = read_file_lines(filepath, &new_lines);
            detect_changes_and_enqueue(old_lines, old_line_count, new_lines, new_count);

            free_lines(old_lines, old_line_count);
            old_lines = new_lines;
            old_line_count = new_count;
        }
        usleep(100000); // sleep 100ms
    }
    return NULL;
}

// Main implementations
bool watcher_init(const char* path, SyncQueue* out_queue, int user_id) {
    if (!path || !out_queue)
        return false;

    //Init global variables to vars from function
    strncpy(filepath, path, sizeof(filepath) - 1);
    filepath[sizeof(filepath) - 1] = '\0';
    queue = out_queue;
    watcher_user_id = user_id;
    old_line_count = read_file_lines(filepath, &old_lines);
    running = false;

    return true;
}

void watcher_start(int interval_ms) {
    running = true;
    pthread_create(&watcher_thread, NULL, watcher_loop, NULL);
}

void watcher_stop(void) {
    running = false;
    pthread_join(watcher_thread, NULL);
    free_lines(old_lines, old_line_count);
    old_lines = NULL;
    old_line_count = 0;
    queue = NULL;
}