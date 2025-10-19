#include "broadcaster.h"
#include "registry.h"
#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#define OP_BUFFER_SIZE 10
#define BATCH_SIZE 1  // Send immediately

static Operation op_buffer[OP_BUFFER_SIZE];
static volatile int op_count = 0;
static pthread_t broadcaster_thread;
static volatile int running = 0;

static int peer_ids[MAX_USERS];
static char peer_queues[MAX_USERS][64];
static int peer_count = 0;

static void send_to_peers(const Operation* op) {
    for (int i = 0; i < peer_count; i++) {
        mqd_t mq = mq_open(peer_queues[i], O_WRONLY | O_NONBLOCK);
        if (mq != (mqd_t)-1) {
            int result = mq_send(mq, (const char*)op, sizeof(Operation), 0);
            if (result == 0) {
                printf("[DEBUG] Sent operation to %s (line %d)\n", peer_queues[i], op->line_no);
            } else {
                printf("[DEBUG] Failed to send to %s: %s\n", peer_queues[i], strerror(errno));
            }
            mq_close(mq);
        } else {
            printf("[DEBUG] Could not open queue %s: %s\n", peer_queues[i], strerror(errno));
        }
    }
}

static void* broadcaster_loop(void* arg) {
    (void)arg;
    
    while (running) {
        if (op_count > 0) {  // Send any operation in buffer immediately
            for (int i = 0; i < op_count; i++) {
                send_to_peers(&op_buffer[i]);
            }
            op_count = 0;
        }
        usleep(100000);  // Check every 0.1 seconds
    }
    return NULL;
}

bool broadcaster_init(void) {
    op_count = 0;
    peer_count = 0;
    return true;
}

void broadcaster_add_operation(const Operation* op) {
    if (op_count < OP_BUFFER_SIZE) {
        memcpy(&op_buffer[op_count], op, sizeof(Operation));
        op_count++;
    }
}

void broadcaster_start(void) {
    running = 1;
    pthread_create(&broadcaster_thread, NULL, broadcaster_loop, NULL);
}

void broadcaster_stop(void) {
    running = 0;
    pthread_join(broadcaster_thread, NULL);
}

void broadcaster_update_peers(int user_ids[], char queue_names[][64], int count) {
    peer_count = count;
    for (int i = 0; i < count; i++) {
        peer_ids[i] = user_ids[i];
        strncpy(peer_queues[i], queue_names[i], 64);
    }
}