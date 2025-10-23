#include "broadcaster.h"
#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_SEND_RETRIES 5
#define SEND_RETRY_DELAY_US 20000  // 20 milliseconds

static int peer_ids[MAX_USERS];
static char peer_queues[MAX_USERS][64];
static int peer_count = 0;

static int reliable_mq_send(mqd_t mq, const char* msg, size_t msg_len) {
    int retries = 0;
    while (retries < MAX_SEND_RETRIES) {
        if (mq_send(mq, msg, msg_len, 0) == 0) {
            return 0; // Success
        }
        if (errno == EAGAIN) {
            usleep(SEND_RETRY_DELAY_US);
            retries++;
        } else {
            // Other fatal errors
            break;
        }
    }
    return -1; // Failed after retries
}

static void send_to_peers(const Operation* op) {
    if (!op) return;

    for (int i = 0; i < peer_count; i++) {
        mqd_t mq = mq_open(peer_queues[i], O_WRONLY | O_NONBLOCK);
        if (mq == (mqd_t)-1) {
            fprintf(stderr, "[BROADCASTER] Could not open queue %s: %s\n",
                    peer_queues[i], strerror(errno));
            continue;
        }

        if (reliable_mq_send(mq, (const char*)op, sizeof(Operation)) == 0) {
            printf("[BROADCASTER] Sent operation to %s (line %d)\n",
                   peer_queues[i], op->line_no);
        } else {
            fprintf(stderr, "[BROADCASTER] Failed to send to %s after retries\n",
                    peer_queues[i]);
        }
        mq_close(mq);
    }
}

bool broadcaster_init(void) {
    peer_count = 0;
    printf("[BROADCASTER] Initialized successfully\n");
    return true;
}

void broadcaster_add_operation(const Operation* op) {
    if (!op) return;
    send_to_peers(op);
}

void broadcaster_start(void) {
    printf("[BROADCASTER] Started (no buffer, immediate send)\n");
}

void broadcaster_stop(void) {
    printf("[BROADCASTER] Stopped\n");
}

void broadcaster_update_peers(int user_ids[], char queue_names[][64], int count) {
    if (!user_ids || !queue_names || count < 0) return;
    peer_count = count;
    for (int i = 0; i < count && i < MAX_USERS; i++) {
        peer_ids[i] = user_ids[i];
        strncpy(peer_queues[i], queue_names[i], 63);
        peer_queues[i][63] = '\0';
    }
    printf("[BROADCASTER] Updated peer list: %d peers\n", peer_count);
}