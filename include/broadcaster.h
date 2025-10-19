#ifndef BROADCASTER_H
#define BROADCASTER_H

#include <stdbool.h>
#include "operation.h"

bool broadcaster_init(void);
void broadcaster_add_operation(const Operation* op);
void broadcaster_start(void);
void broadcaster_stop(void);
void broadcaster_update_peers(int user_ids[], char queue_names[][64], int count);

#endif