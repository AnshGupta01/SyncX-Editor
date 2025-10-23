#include "utils.h"
#include <stdio.h>
#include <time.h>

void utils_display_document(const char* doc_name, int user_id, char** lines, size_t line_count,
                             int active_users[], int active_count, const char* last_change) {
    if (!doc_name || !lines || !active_users || !last_change) {
        return;
    }
    
    printf("\033[H\033[J");  // Clear screen
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    
    printf("========================================\n");
    printf("  SyncText Collaborative Editor\n");
    printf("========================================\n");
    printf("Document: %s\n", doc_name);
    printf("User ID: user_%d\n", user_id);
    printf("Last updated: %s\n", time_buf);
    printf("----------------------------------------\n\n");
    
    for (size_t i = 0; i < line_count; i++) {
        printf("Line %zu: %s", i, lines[i] ? lines[i] : "\n");
    }
    
    printf("\n----------------------------------------\n");
    printf("Active users: ");
    for (int i = 0; i < active_count; i++) {
        printf("user_%d%s", active_users[i], (i < active_count - 1) ? ", " : "");
    }
    printf("\n");
    
    printf("\n%s\n", last_change);
    
    printf("\nMonitoring for changes...\n");
    printf("========================================\n");
    fflush(stdout);
}