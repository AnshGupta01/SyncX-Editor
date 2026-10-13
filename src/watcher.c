#include "watcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

static char* filepath = NULL;
static int user_id_global = 0;
static pthread_t watcher_thread;
static volatile bool running = false;
static volatile bool self_write_pending = false;
static OperationCallback op_callback = NULL;

static char** old_lines = NULL;
static size_t old_line_count = 0;

void watcher_note_self_write(void) {
    self_write_pending = true;
}

static size_t read_file_lines(const char* path, char*** lines_out) {
    if (!path || !lines_out) {
        fprintf(stderr, "[WATCHER] NULL parameter in read_file_lines\n");
        return 0;
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        printf("[WATCHER] Cannot open file %s\n", path);
        *lines_out = NULL;
        return 0;
    }

    char** lines = NULL;
    size_t count = 0;
    char* line = NULL;
    size_t len = 0;
    ssize_t nread;

    while ((nread = getline(&line, &len, f)) != -1) {
        char** temp = realloc(lines, (count + 1) * sizeof(char*));
        if (!temp) {
            fprintf(stderr, "[WATCHER] Memory allocation failed\n");
            free(line);
            fclose(f);
            for (size_t i = 0; i < count; i++) {
                free(lines[i]);
            }
            free(lines);
            return 0;
        }
        lines = temp;
        lines[count] = strdup(line);
        if (!lines[count]) {
            fprintf(stderr, "[WATCHER] String allocation failed\n");
            free(line);
            fclose(f);
            for (size_t i = 0; i < count; i++) {
                free(lines[i]);
            }
            free(lines);
            return 0;
        }
        count++;
    }

    free(line);
    fclose(f);

    *lines_out = lines;
    return count;
}

static void free_lines(char** lines, size_t count) {
    if (!lines) return;

    for (size_t i = 0; i < count; i++) {
        if (lines[i]) free(lines[i]);
    }
    free(lines);
}

static void find_column_diff(const char* old_str, const char* new_str,
                             int* col_start, int* col_end) {
    if (!old_str || !new_str || !col_start || !col_end) {
        if (col_start) *col_start = 0;
        if (col_end) *col_end = 0;
        return;
    }

    int len_old = strlen(old_str);
    int len_new = strlen(new_str);

    int start = 0;
    while (start < len_old && start < len_new && old_str[start] == new_str[start]) {
        start++;
    }

    int end_old = len_old - 1;
    int end_new = len_new - 1;

    while (end_old >= start && end_new >= start && old_str[end_old] == new_str[end_new]) {
        end_old--;
        end_new--;
    }

    *col_start = start;
    *col_end = (end_new > end_old) ? end_new : end_old;
}

static void detect_changes_and_callback(char** old, size_t old_count,
                                        char** new_lines, size_t new_count) {
    if (!op_callback) return;

    size_t max_count = (old_count > new_count) ? old_count : new_count;

    for (size_t i = 0; i < max_count; i++) {
        const char* old_line = (i < old_count && old && old[i]) ? old[i] : "";
        const char* new_line = (i < new_count && new_lines && new_lines[i]) ? new_lines[i] : "";

        if (strcmp(old_line, new_line) != 0) {
            Operation op;
            memset(&op, 0, sizeof(Operation));

            op.user_id = user_id_global;
            op.line_no = (int)i;
            op.timestamp = time(NULL);

            if (i >= old_count) {
                op.op_type = 'I';
                op.col_start = 0;
                op.col_end = (int)strlen(new_line);
                op.old_text[0] = '\0';  // Empty string instead of snprintf with ""
                snprintf(op.new_text, MAX_TEXT_LEN, "%s", new_line);
            }
            else if (i >= new_count) {
                op.op_type = 'D';
                op.col_start = 0;
                op.col_end = (int)strlen(old_line);
                snprintf(op.old_text, MAX_TEXT_LEN, "%s", old_line);
                op.new_text[0] = '\0';  // Empty string instead of snprintf with ""
            }
            else {
                op.op_type = 'R';
                find_column_diff(old_line, new_line, &op.col_start, &op.col_end);
                snprintf(op.old_text, MAX_TEXT_LEN, "%s", old_line);
                snprintf(op.new_text, MAX_TEXT_LEN, "%s", new_line);
            }

            printf("[WATCHER] Detected change: line %d, cols %d-%d, type '%c'\n",
                   op.line_no, op.col_start, op.col_end, op.op_type);

            op_callback(&op);
        }
    }
}

/* Refresh snapshot from disk; update *last_mod_time. No op callbacks. */
static void refresh_baseline_no_broadcast(time_t* last_mod_time) {
    if (!filepath || !last_mod_time) return;

    struct stat st;
    if (stat(filepath, &st) == 0) {
        *last_mod_time = st.st_mtime;
    }

    char** new_lines = NULL;
    size_t new_count = read_file_lines(filepath, &new_lines);

    free_lines(old_lines, old_line_count);
    old_lines = new_lines;
    old_line_count = new_count;

    printf("[WATCHER] Self-write — baseline refreshed, no broadcast\n");
}

static void* watcher_loop(void* arg) {
    (void)arg;
    time_t last_mod_time = 0;

    printf("[WATCHER] Thread started\n");

    while (running) {
        if (!filepath) {
            usleep(500000);
            continue;
        }

        /* CRDT rewrote the file — adopt new baseline; do not rebroadcast. */
        if (self_write_pending) {
            self_write_pending = false;
            refresh_baseline_no_broadcast(&last_mod_time);
            usleep(500000);
            continue;
        }

        struct stat st;
        if (stat(filepath, &st) == 0 && st.st_mtime != last_mod_time) {
            last_mod_time = st.st_mtime;

            char** new_lines = NULL;
            size_t new_count = read_file_lines(filepath, &new_lines);

            detect_changes_and_callback(old_lines, old_line_count, new_lines, new_count);

            free_lines(old_lines, old_line_count);
            old_lines = new_lines;
            old_line_count = new_count;
        }

        usleep(500000);
    }

    printf("[WATCHER] Thread stopped\n");
    return NULL;
}

bool watcher_init(const char* file_path, int user_id, OperationCallback callback) {
    if (!file_path || !callback) {
        fprintf(stderr, "[WATCHER] NULL parameter in watcher_init\n");
        return false;
    }

    printf("[WATCHER] Initializing for %s...\n", file_path);

    filepath = strdup(file_path);
    if (!filepath) {
        fprintf(stderr, "[WATCHER] String allocation failed\n");
        return false;
    }

    user_id_global = user_id;
    op_callback = callback;

    old_lines = NULL;
    old_line_count = read_file_lines(filepath, &old_lines);

    printf("[WATCHER] Initialized with %zu lines\n", old_line_count);
    return true;
}

void watcher_start(void) {
    printf("[WATCHER] Starting thread...\n");
    running = true;
    if (pthread_create(&watcher_thread, NULL, watcher_loop, NULL) != 0) {
        fprintf(stderr, "[WATCHER] Failed to create thread\n");
        running = false;
    }
}

void watcher_stop(void) {
    printf("[WATCHER] Stopping thread...\n");
    running = false;
    pthread_join(watcher_thread, NULL);

    if (filepath) {
        free(filepath);
        filepath = NULL;
    }
    free_lines(old_lines, old_line_count);
    old_lines = NULL;
    old_line_count = 0;
    printf("[WATCHER] Stopped successfully\n");
}