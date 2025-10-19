#include "utils.h"
#include <stdio.h>
#include <time.h>

void utils_display_document(const char* doc_name, int user_id, char** lines, size_t line_count, 
                           int active_users[], int active_count, const char* last_change) {
    printf("\033[H\033[J");  // Clear screen
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    
    printf("Document: %s\n", doc_name);
    printf("Last updated: %s\n", time_buf);
    printf("----------------------------------------\n");
    
    for (size_t i = 0; i < line_count; i++) {
        printf("Line %zu: %s", i, lines[i]);
    }
    
    printf("----------------------------------------\n");
    printf("Active users: ");
    for (int i = 0; i < active_count; i++) {
        printf("user_%d%s", active_users[i], (i < active_count - 1) ? ", " : "");
    }
    printf("\n");
    
    if (last_change) {
        printf("%s\n", last_change);
    }
    
    printf("Monitoring for changes...\n");
    fflush(stdout);
}