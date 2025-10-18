// Contains timestamp generation, string utilities, 
// file read/write helpers, and terminal display routines.

#include "utils.h"
#include "crdt.h"
#include <stdio.h>

void utils_refresh_terminal_display(void) {
    printf("\033[H\033[J");

    char** document_lines = crdt_get_document_lines();
    size_t line_count = crdt_get_line_count();

    for (size_t i = 0; i < line_count; i++) {
        printf("%s", document_lines[i]);
    }

    fflush(stdout);
}