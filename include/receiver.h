#ifndef RECEIVER_H
#define RECEIVER_H
#include <stdbool.h>
#include "operation.h"

typedef void (*MergeCallback)(const Operation* op);

bool receiver_init(int user_id, MergeCallback callback);
void receiver_start(void);
void receiver_stop(void);

#endif