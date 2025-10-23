#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H
#include <stdbool.h>
#include "operation.h"
#define SYNC_QUEUE_SIZE 1024

typedef struct SyncQueue SyncQueue;

SyncQueue* sync_queue_create(void);
void sync_queue_destroy(SyncQueue* queue);
bool sync_queue_enqueue(SyncQueue* queue, const Operation* op);
bool sync_queue_dequeue(SyncQueue* queue, Operation* op);

#endif