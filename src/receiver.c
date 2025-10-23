#include "receiver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>  // For offsetof

static mqd_t mq = (mqd_t)-1;
static char queue_name[64];
static pthread_t receiver_thread;
static volatile int running = 0;
static MergeCallback merge_callback = NULL;

static void* receiver_loop(void* arg) {
    (void)arg;

    printf("[RECEIVER] Thread started\n");

    while (running) {
        if (mq == (mqd_t)-1) {
            usleep(10000);
            continue;
        }

        Operation op;
        ssize_t bytes_read = mq_receive(mq, (char*)&op, sizeof(Operation), NULL);

        if (bytes_read > 0) {
            printf("[RECEIVER] Received operation from user_%d for line %d\n", op.user_id, op.line_no);

            if (merge_callback) {
                merge_callback(&op);  // Immediately process each operation on receipt
            }
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            if (running) {
                fprintf(stderr, "[RECEIVER] Error receiving: %s\n", strerror(errno));
            }
        }

        usleep(10000);
    }

    printf("[RECEIVER] Thread stopped\n");
    return NULL;
}

bool receiver_init(int user_id, MergeCallback callback) {
    if (!callback) {
        fprintf(stderr, "[RECEIVER] NULL callback\n");
        return false;
    }

    snprintf(queue_name, sizeof(queue_name), "/queue_user_%d", user_id);
    merge_callback = callback;

    mq_unlink(queue_name);

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;  // safe reduced maxmsg
    attr.mq_msgsize = sizeof(Operation);
    attr.mq_curmsgs = 0;

    printf("[RECEIVER] Operation struct size: %zu bytes\n", sizeof(Operation));

    mq = mq_open(queue_name, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("[RECEIVER] mq_open");
        return false;
    }

    printf("[RECEIVER] Initialized queue %s successfully\n", queue_name);
    return true;
}

void receiver_start(void) {
    running = 1;
    if (pthread_create(&receiver_thread, NULL, receiver_loop, NULL) != 0) {
        fprintf(stderr, "[RECEIVER] Failed to create thread\n");
        running = 0;
    } else {
        printf("[RECEIVER] Started successfully\n");
    }
}

void receiver_stop(void) {
    printf("[RECEIVER] Stopping...\n");
    running = 0;
    pthread_join(receiver_thread, NULL);

    if (mq != (mqd_t)-1) {
        mq_close(mq);
        mq_unlink(queue_name);
        mq = (mqd_t)-1;
    }

    printf("[RECEIVER] Stopped successfully\n");
}