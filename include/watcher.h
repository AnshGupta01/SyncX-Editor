#ifndef WATCHER_H
#define WATCHER_H

#include <stdbool.h>
#include "operation.h"

typedef void (*OperationCallback)(const Operation* op);

bool watcher_init(const char* file_path, int user_id, OperationCallback callback);
void watcher_start(void);
void watcher_stop(void);

/* Call before and after crdt_save_to_file when we wrote the file ourselves
 * (remote merge or local CRDT rewrite). Watcher refreshes its baseline and
 * does not treat that mtime change as a user edit (no rebroadcast / echo). */
void watcher_note_self_write(void);

#endif
