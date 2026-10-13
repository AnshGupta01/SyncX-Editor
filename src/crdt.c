#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "crdt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

static char** document_lines = NULL;
static size_t line_count = 0;
static size_t line_capacity = 0;

typedef struct {
    time_t timestamp;
    int user_id;
} LineMetadata;

static LineMetadata* line_metadata = NULL;

/* Serializes watcher + receiver (and UI) access to the in-process CRDT heap. */
static pthread_mutex_t crdt_mu = PTHREAD_MUTEX_INITIALIZER;

void crdt_lock(void) {
    pthread_mutex_lock(&crdt_mu);
}

void crdt_unlock(void) {
    pthread_mutex_unlock(&crdt_mu);
}

static void ensure_line_capacity(size_t n) {
    if (n == 0) return;

    if (line_capacity < n) {
        size_t old_capacity = line_capacity;
        line_capacity = n + 20;

        char** new_doc_lines = realloc(document_lines, line_capacity * sizeof(char*));
        LineMetadata* new_meta = realloc(line_metadata, line_capacity * sizeof(LineMetadata));

        if (!new_doc_lines || !new_meta) {
            fprintf(stderr, "[CRDT] Memory allocation failed\n");
            exit(1);
        }

        document_lines = new_doc_lines;
        line_metadata = new_meta;

        for (size_t i = old_capacity; i < line_capacity; i++) {
            document_lines[i] = NULL;
            line_metadata[i].timestamp = 0;
            line_metadata[i].user_id = -1;
        }
    }

    while (line_count < n) {
        if (document_lines[line_count] == NULL) {
            document_lines[line_count] = strdup("\n");
            if (!document_lines[line_count]) {
                fprintf(stderr, "[CRDT] String allocation failed at line %zu\n", line_count);
                exit(1);
            }
            line_metadata[line_count].timestamp = 0;
            line_metadata[line_count].user_id = -1;
        }
        line_count++;
    }
}

static void shrink_document(void) {
    while(line_count > 0 && document_lines[line_count - 1] && strcmp(document_lines[line_count - 1], "\n") == 0) {
        free(document_lines[line_count - 1]);
        document_lines[line_count - 1] = NULL;
        line_metadata[line_count - 1].timestamp = 0;
        line_metadata[line_count - 1].user_id = -1;
        line_count--;
    }
}

void crdt_init(void) {
    pthread_mutex_lock(&crdt_mu);
    if (document_lines) {
        for (size_t i = 0; i < line_capacity; i++) {
            free(document_lines[i]);
        }
        free(document_lines);
        document_lines = NULL;
    }
    if (line_metadata) {
        free(line_metadata);
        line_metadata = NULL;
    }
    line_count = 0;
    line_capacity = 0;
    pthread_mutex_unlock(&crdt_mu);
}

void crdt_destroy(void) {
    pthread_mutex_lock(&crdt_mu);
    if (document_lines) {
        for (size_t i = 0; i < line_capacity; i++) {
            free(document_lines[i]);
        }
        free(document_lines);
        document_lines = NULL;
    }
    if (line_metadata) {
        free(line_metadata);
        line_metadata = NULL;
    }
    line_count = 0;
    line_capacity = 0;
    pthread_mutex_unlock(&crdt_mu);
}

static bool should_apply_operation(const Operation* op, int line_no) {
    if (line_no < 0 || (size_t)line_no >= line_count) {
        return true;
    }
    if (!line_metadata) return true;

    LineMetadata* meta = &line_metadata[line_no];

    if (meta->timestamp == 0) {
        return true;
    }
    if (op->timestamp > meta->timestamp) {
        return true;
    } else if (op->timestamp == meta->timestamp) {
        return op->user_id < meta->user_id;
    }
    return false;
}

static bool merge_operation_unlocked(const Operation* op) {
    if (!op || op->line_no < 0) {
        fprintf(stderr, "[CRDT] Invalid operation or line number\n");
        return false;
    }

    ensure_line_capacity((size_t)op->line_no + 1);

    if (!should_apply_operation(op, op->line_no)) {
        return false;
    }

    if (document_lines == NULL || line_metadata == NULL) {
        fprintf(stderr, "[CRDT] Internal structures not initialized\n");
        return false;
    }

    if (op->op_type == 'R' || op->op_type == 'I') {
        if (document_lines[op->line_no]) {
            free(document_lines[op->line_no]);
        }
        document_lines[op->line_no] = strdup(op->new_text);

        if (!document_lines[op->line_no]) {
            fprintf(stderr, "[CRDT] String allocation failed\n");
            return false;
        }

        line_metadata[op->line_no].timestamp = op->timestamp;
        line_metadata[op->line_no].user_id = op->user_id;

        return true;
    } else if (op->op_type == 'D') {
        if (document_lines[op->line_no]) {
            free(document_lines[op->line_no]);
        }
        document_lines[op->line_no] = strdup("\n");

        if (!document_lines[op->line_no]) {
            fprintf(stderr, "[CRDT] String allocation failed\n");
            return false;
        }

        line_metadata[op->line_no].timestamp = op->timestamp;
        line_metadata[op->line_no].user_id = op->user_id;

        return true;
    }

    return false;
}

static void save_to_file_unlocked(const char* filepath) {
    if (!filepath) return;
    FILE* f = fopen(filepath, "w");
    if (!f) return;

    for (size_t i = 0; i < line_count; i++) {
        if (document_lines[i]) {
            fputs(document_lines[i], f);
        } else {
            fputs("\n", f);
        }
    }

    fclose(f);
}

bool crdt_merge_operation(const Operation* op) {
    pthread_mutex_lock(&crdt_mu);
    bool ok = merge_operation_unlocked(op);
    pthread_mutex_unlock(&crdt_mu);
    return ok;
}

bool crdt_merge_and_save(const Operation* op, const char* filepath) {
    pthread_mutex_lock(&crdt_mu);
    bool ok = merge_operation_unlocked(op);
    /* Always rewrite file under the same lock (matches prior merge-then-save). */
    save_to_file_unlocked(filepath);
    pthread_mutex_unlock(&crdt_mu);
    return ok;
}

int crdt_merge_batch(Operation ops[], int count) {
    if (count <= 0 || !ops) return 0;

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (ops[i].timestamp > ops[j].timestamp ||
                (ops[i].timestamp == ops[j].timestamp && ops[i].user_id > ops[j].user_id)) {
                Operation temp = ops[i];
                ops[i] = ops[j];
                ops[j] = temp;
            }
        }
    }

    pthread_mutex_lock(&crdt_mu);
    int applied = 0;
    for (int i = 0; i < count; i++) {
        if (merge_operation_unlocked(&ops[i])) {
            applied++;
        }
    }
    shrink_document();
    pthread_mutex_unlock(&crdt_mu);

    return applied;
}

char** crdt_get_document_lines(void) {
    return document_lines;
}

size_t crdt_get_line_count(void) {
    return line_count;
}

void crdt_save_to_file(const char* filepath) {
    pthread_mutex_lock(&crdt_mu);
    save_to_file_unlocked(filepath);
    pthread_mutex_unlock(&crdt_mu);
}

void crdt_load_from_file(const char* filepath) {
    if (!filepath) return;
    FILE* f = fopen(filepath, "r");
    if (!f) return;

    pthread_mutex_lock(&crdt_mu);

    char buffer[4096];
    size_t idx = 0;

    while (fgets(buffer, sizeof(buffer), f)) {
        ensure_line_capacity(idx + 1);
        if (document_lines[idx]) free(document_lines[idx]);
        document_lines[idx] = strdup(buffer);
        if (line_metadata) {
            line_metadata[idx].timestamp = time(NULL);
            line_metadata[idx].user_id = -1;
        }
        idx++;
    }
    line_count = idx;

    pthread_mutex_unlock(&crdt_mu);
    fclose(f);
}
