#ifndef CRDT_H
#define CRDT_H

#include <stdbool.h>
#include <stddef.h>
#include "operation.h"

void crdt_init(void);
void crdt_destroy(void);
bool crdt_merge_operation(const Operation* op);
/* Merge then save under one mutex — preferred path for watcher/receiver. */
bool crdt_merge_and_save(const Operation* op, const char* filepath);
char** crdt_get_document_lines(void);
size_t crdt_get_line_count(void);
void crdt_save_to_file(const char* filepath);
void crdt_load_from_file(const char* filepath);
int crdt_merge_batch(Operation ops[], int count);

/* Hold across UI reads of get_document_lines / get_line_count. */
void crdt_lock(void);
void crdt_unlock(void);

#endif