#include "syncQueue.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

struct SyncQueue {
    Operation buffer[SYNC_QUEUE_SIZE];
    atomic_size_t head;
    atomic_size_t tail;
};

SyncQueue* sync_queue_create(void) {
    SyncQueue* q = (SyncQueue*)malloc(sizeof(SyncQueue));
    if (q) {
        atomic_init(&q->head, 0);
        atomic_init(&q->tail, 0);
        memset(q->buffer, 0, sizeof(q->buffer));
    }
    return q;
}

void sync_queue_destroy(SyncQueue* queue) {
    if (queue) {
        free(queue);
    }
}

bool sync_queue_enqueue(SyncQueue* queue, const Operation* op) {
    if (!queue || !op) return false;
    
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    size_t next_tail = (tail + 1) % SYNC_QUEUE_SIZE;
    
    if (next_tail == head) return false;  // Queue full
    
    memcpy(&queue->buffer[tail], op, sizeof(Operation));
    atomic_store_explicit(&queue->tail, next_tail, memory_order_release);
    
    return true;
}

bool sync_queue_dequeue(SyncQueue* queue, Operation* op) {
    if (!queue || !op) return false;
    
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    
    if (head == tail) return false;  // Queue empty
    
    memcpy(op, &queue->buffer[head], sizeof(Operation));
    atomic_store_explicit(&queue->head, (head + 1) % SYNC_QUEUE_SIZE, memory_order_release);
    
    return true;
}