// Defines the core Operation struct with fields:
// (op_type, line_no, col_start, col_end, new_text, timestamp, user_id)
// Will help to serialize and de-serialize -> struct->byteArray->struct

#ifndef OPERATION_H
#define OPERATION_H

#include <time.h>
#include <stddef.h>

#define MAX_TEXT_LENGTH 128


typedef struct {
    char op_type;       // Insert ('I'), Delete ('D'), Replace ('R'), use R for simplicity
    int line_no;
    int col_start;
    int col_end;
    char new_text[MAX_TEXT_LENGTH];
    time_t timestamp;
    int user_id;
} Operation;

// Struct to byte buffer
void serialize_operation(const Operation* op, char* buffer);

// Byte buffer to struct
void deserialize_operation(const char* buffer, Operation* op);

size_t get_serialized_operation_size(void);

#endif