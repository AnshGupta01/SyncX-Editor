//Sends queued operations to all connected peers using POSIX message queues.

#ifndef BROADCASTER_H
#define BROADCASTER_H
#include <stdbool.h>
#include "operation.h"
#include "syncQueue.h"

// Initializes broadcaster with:
// - Array of peer queue names (excluding this user)
// - Number of peers
// - Pointer to the operation queue (produced by watcher)
bool broadcaster_init(const char** peer_queues, int peer_count, SyncQueue* in_queue);
void broadcaster_start(void);
void broadcaster_stop(void);

#endif