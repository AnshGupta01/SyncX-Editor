// Runs listening thread using mq_receive() to accept remote operations and trigger CRDT merges.

#ifndef RECEIVER_H
#define RECEIVER_H

#include <stdbool.h>
#include "operation.h"
#include "syncQueue.h"

bool receiver_init(int user_id, SyncQueue* in_queue);
void receiver_start(void);
void receiver_stop(void);

#endif