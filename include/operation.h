// Defines the core Operation struct with fields:
// (op_type, line_no, col_start, col_end, new_text, timestamp, user_id)
// Will help to serialize and de-serialize -> struct->byteArray->struct

#ifndef OPERATION_H
#define OPERATION_H

#include <time.h>
#include <stddef.h>

#define MAX_TEXT_LENGTH 128


typedef struct {
    int user_id;              // Which user made the edit
    char op_type;             // 'R' for replace
    int line_no;              // Line number changed
    int col_start;            // Reserved for detailed changes
    int col_end;              // Reserved for detailed changes
    char old_text[1024];      // Stores the pre-change content
    char new_text[1024];      // Stores the post-change content
    time_t timestamp;         // When the change was made
} Operation;

// Struct to byte buffer
void serialize_operation(const Operation* op, char* buffer);

// Byte buffer to struct
void deserialize_operation(const char* buffer, Operation* op);

size_t get_serialized_operation_size(void);

#endif