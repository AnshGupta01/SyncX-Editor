#include "utils.h"
#include "crdt.h"
#include <stdio.h>
#include <string.h>

void utils_refresh_terminal_display(void) {
    printf("\033[H\033[J");
    printf("=== SyncText Document ===\n\n");

    char** document_lines = crdt_get_document_lines();
    size_t line_count = crdt_get_line_count();

    for (size_t i = 0; i < line_count; i++) {
        printf("%zu: %s", i + 1, document_lines[i]);
        size_t len = strlen(document_lines[i]);
        if (len > 0 && document_lines[i][len - 1] != '\n') {
            printf("\n");
        }
    }

    printf("\n=== End of Document ===\n");
    fflush(stdout);
}