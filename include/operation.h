#ifndef OPERATION_H
#define OPERATION_H

#include <time.h>

#define MAX_TEXT_LEN 3072  // Increased from 1024 for larger changes

typedef struct {
    int user_id;
    char op_type;  // 'I' = insert, 'D' = delete, 'R' = replace
    int line_no;
    int col_start;
    int col_end;
    char old_text[MAX_TEXT_LEN];
    char new_text[MAX_TEXT_LEN];
    time_t timestamp;
} Operation;

// Serialization functions
void serialize_operation(const Operation* op, char* buffer);
void deserialize_operation(const char* buffer, Operation* op);
size_t get_serialized_operation_size(void);

#endif