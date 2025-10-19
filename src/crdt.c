#include "crdt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char** document_lines = NULL;
static size_t line_count = 0;
static size_t line_capacity = 0;

static void ensure_line_capacity(size_t n) {
    if (line_capacity < n) {
        size_t old_capacity = line_capacity;
        line_capacity = n + 10;
        
        document_lines = realloc(document_lines, line_capacity * sizeof(char*));
        for (size_t i = old_capacity; i < line_capacity; i++) {
            document_lines[i] = NULL;
        }
    }
    
    if (line_count < n) {
        for (size_t i = line_count; i < n; i++) {
            if (!document_lines[i]) {
                document_lines[i] = strdup("\n");
            }
        }
        line_count = n;
    }
}

void crdt_init(void) {
    document_lines = NULL;
    line_count = 0;
    line_capacity = 0;
}

void crdt_destroy(void) {
    for (size_t i = 0; i < line_capacity; i++) {
        free(document_lines[i]);
    }
    free(document_lines);
    document_lines = NULL;
    line_count = 0;
    line_capacity = 0;
}

bool crdt_merge_operation(const Operation* op) {
    if (op->line_no < 0) return false;

    ensure_line_capacity((size_t)op->line_no + 1);

    if (op->op_type == 'R') {
        free(document_lines[op->line_no]);
        document_lines[op->line_no] = strdup(op->new_text);
        return true;
    }
    
    return false;
}

char** crdt_get_document_lines(void) {
    return document_lines;
}

size_t crdt_get_line_count(void) {
    return line_count;
}

void crdt_save_to_file(const char* filepath) {
    FILE* f = fopen(filepath, "w");
    if (!f) {
        perror("Failed to save document");
        return;
    }
    
    for (size_t i = 0; i < line_count; i++) {
        if (document_lines[i]) {
            fputs(document_lines[i], f);
        }
    }
    
    fclose(f);
}

void crdt_load_from_file(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) return;
    
    char* line = NULL;
    size_t len = 0;
    ssize_t nread;
    size_t idx = 0;

    while ((nread = getline(&line, &len, f)) != -1) {
        ensure_line_capacity(idx + 1);
        free(document_lines[idx]);
        document_lines[idx] = strdup(line);
        idx++;
    }

    free(line);
    fclose(f);
}