#ifndef UTILS_H
#define UTILS_H
#include <stddef.h>

void utils_display_document(const char* doc_name, int user_id, char** lines, size_t line_count,int active_users[], int active_count, const char* last_change);

#endif