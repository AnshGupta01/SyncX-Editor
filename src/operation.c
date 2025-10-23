#include "operation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

void serialize_operation(const Operation* op, char* buffer) {
    if (!op || !buffer) return;
    memcpy(buffer, op, sizeof(Operation));
}

void deserialize_operation(const char* buffer, Operation* op) {
    if (!buffer || !op) return;
    memcpy(op, buffer, sizeof(Operation));
}

size_t get_serialized_operation_size(void) {
    return sizeof(Operation);
}