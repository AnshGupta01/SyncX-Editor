// Implements a lock-free thread-safe circular queue for operations to pass between threads.

#include "syncQueue.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

//Its kinda a circular queue
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
    }
    return q;
}

void sync_queue_destroy(SyncQueue* queue) {
    if (queue) {
        free(queue);
    }
}

bool sync_queue_enqueue(SyncQueue* queue, const Operation* op) {
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    // (Pointer to load from, Mem Order required)
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    size_t next_tail = (tail + 1) % SYNC_QUEUE_SIZE;

    // Queue is full 
    if (next_tail == head) return false;

    // Copy operation into buffer at tail index
    memcpy(&queue->buffer[tail], op, sizeof(Operation));

    // Update tail pointer -> (Value, desired, Memory order)
    atomic_store_explicit(&queue->tail, next_tail, memory_order_release);
    return true;
}

bool sync_queue_dequeue(SyncQueue* queue, Operation* op) {
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);

    // Queue is empty
    if (head == tail) return false;

    // Copy operation from buffer at head index
    memcpy(op, &queue->buffer[head], sizeof(Operation));
    // Publish new head index
    atomic_store_explicit(&queue->head, (head + 1) % SYNC_QUEUE_SIZE, memory_order_release);
    return true;
}

//Release -> Ensure no prior writes in this thread are ordered after this read
//Relaxed -> chill order, doesnt matter in this thread
//Acquire -> No read/ write in this thread can be moved after this load