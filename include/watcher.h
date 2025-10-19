#ifndef WATCHER_H
#define WATCHER_H

#include <stdbool.h>
#include "operation.h"

typedef void (*OperationCallback)(const Operation* op);

bool watcher_init(const char* file_path, int user_id, OperationCallback callback);
void watcher_start(void);
void watcher_stop(void);

#endif