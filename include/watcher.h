// Handles file monitoring (using stat()), detects changed lines, computes differences, and generates Operation objects.

#ifndef WATCHER_H
#define WATCHER_H

#include <stdbool.h>
#include "operation.h"
#include "syncQueue.h"

bool watcher_init(const char* filepath, SyncQueue* out_queue, int user_id);

// Detects changes every interval_ms milliseconds and pushes Operation objects to out_queue
void watcher_start(int interval_ms);

void watcher_stop(void);

#endif