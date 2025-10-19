#include "crdt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char** document_lines = NULL;
static size_t line_count = 0;

static void ensure_line_capacity(size_t n) {
    if (line_count < n) {
        size_t old_count = line_count;
        line_count = n;

        document_lines = realloc(document_lines, line_count * sizeof(char*));
        for (size_t i = old_count; i < line_count; i++) {
            document_lines[i] = strdup("");
        }
    }
}

void crdt_init(void) {
    document_lines = NULL;
    line_count = 0;
}

void crdt_destroy(void) {
    for (size_t i = 0; i < line_count; i++) {
        free(document_lines[i]);
    }
    free(document_lines);
    document_lines = NULL;
    line_count = 0;
}

void crdt_merge_operation(const Operation* op) {
    if (op->line_no < 0) return;

    ensure_line_capacity((size_t)op->line_no + 1);

    if (op->op_type == 'R') {
        free(document_lines[op->line_no]);
        document_lines[op->line_no] = strdup(op->new_text);
    }
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
        perror("Failed to open local doc for writing");
        return;
    }
    
    for (size_t i = 0; i < line_count; i++) {
        fputs(document_lines[i], f);
        size_t len = strlen(document_lines[i]);
        if (len > 0 && document_lines[i][len - 1] != '\n') {
            fputc('\n', f);
        }
    }
    
    fclose(f);
}