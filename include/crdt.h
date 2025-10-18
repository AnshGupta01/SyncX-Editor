// Provides merging logic and conflict resolution (Last Writer Wins).
// Contains crdt_merge_operation() and crdt_conflict_detect() functions.

#ifndef CRDT_H
#define CRDT_H

#include <stdbool.h>
#include "operation.h"

char** crdt_get_document_lines(void);
size_t crdt_get_line_count(void);

void crdt_init(void);
void crdt_destroy(void);
void crdt_merge_operation(const Operation* op);

#endif