// Defines the core Operation struct with fields:
// (op_type, line_no, col_start, col_end, new_text, timestamp, user_id)
// Will help to serialize and de-serialize -> struct->byteArray->struct

#include "operation.h"
#include <string.h>
#include <stddef.h>

void serialize_operation(const Operation* op, char* buffer) {
    memcpy(buffer, op, sizeof(Operation));
}

void deserialize_operation(const char* buffer, Operation* op) {
    memcpy(op, buffer, sizeof(Operation));
}

// This is constant since Operation is a fixed-size struct
size_t get_serialized_operation_size(void) {
    return sizeof(Operation);
}