// Implements a lock-free thread-safe circular queue for operations to pass between threads.

#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H
#include <stdbool.h>
#include "operation.h"
#define SYNC_QUEUE_SIZE 1024

typedef struct SyncQueue SyncQueue; // Opaque queue structure

SyncQueue* sync_queue_create(void);
void sync_queue_destroy(SyncQueue* queue);
bool sync_queue_enqueue(SyncQueue* queue, const Operation* op);
bool sync_queue_dequeue(SyncQueue* queue, Operation* op); //Op stoed in *op

#endif