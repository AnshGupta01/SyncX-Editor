// Provides merging logic and conflict resolution (Last Writer Wins).
// Contains crdt_merge_operation() and crdt_conflict_detect() functions.

#include "crdt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Represent document as dynamic array of lines
static char** document_lines = NULL;
static size_t line_count = 0;

// Add these accessor functions
char** crdt_get_document_lines(void) {
    return document_lines;
}

size_t crdt_get_line_count(void) {
    return line_count;
}


// Helper to expand document to have at least n lines
static void ensure_line_capacity(size_t n) {
    if (line_count < n) {
        size_t old_count = line_count;
        line_count = n;

        document_lines = realloc(document_lines, line_count * sizeof(char*));
        for (size_t i = old_count; i < line_count; i++) {
            document_lines[i] = strdup("");     //Making all of it empty
        }
    }
}

// Initialize document
void crdt_init(void) {
    document_lines = NULL;
    line_count = 0;
}

// Clean resources
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

    // Ensure document has enough lines
    ensure_line_capacity((size_t)op->line_no + 1);

    // Using Replace Operation 'R' smartly
    if (op->op_type == 'R') {
        free(document_lines[op->line_no]);
        document_lines[op->line_no] = strdup(op->new_text);
    }

    // To handle delete and insertion too!!
}