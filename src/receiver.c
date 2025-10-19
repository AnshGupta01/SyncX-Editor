#include "receiver.h"
#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static mqd_t mq = (mqd_t)-1;
static char queue_name[64];
static pthread_t receiver_thread;
static volatile int running = 0;
static MergeCallback merge_callback = NULL;

static void* receiver_loop(void* arg) {
    (void)arg;
    
    while (running) {
        Operation op;
        ssize_t bytes_read = mq_receive(mq, (char*)&op, sizeof(Operation), NULL);
        
        if (bytes_read > 0) {
            printf("[DEBUG] Received operation from user_%d for line %d\n", op.user_id, op.line_no);
            if (merge_callback) {
                merge_callback(&op);
            }
        }
        
        usleep(10000);
    }
    return NULL;
}

bool receiver_init(int user_id, MergeCallback callback) {
    snprintf(queue_name, sizeof(queue_name), "/queue_user_%d", user_id);
    merge_callback = callback;
    
    mq_unlink(queue_name);
    
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(Operation);
    attr.mq_curmsgs = 0;

    mq = mq_open(queue_name, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        return false;
    }

    return true;
}

void receiver_start(void) {
    running = 1;
    pthread_create(&receiver_thread, NULL, receiver_loop, NULL);
}

void receiver_stop(void) {
    running = 0;
    pthread_join(receiver_thread, NULL);
    
    if (mq != (mqd_t)-1) {
        mq_close(mq);
        mq_unlink(queue_name);
    }
}