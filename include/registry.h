#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdbool.h>

#define MAX_USERS 5
#define REGISTRY_NAME "/synctext_registry"
#define QUEUE_NAME_LEN 64

typedef struct {
    int user_id;
    char queue_name[QUEUE_NAME_LEN];
    volatile int active;
} UserEntry;

typedef struct {
    UserEntry users[MAX_USERS];
    volatile int count;
} UserRegistry;

bool registry_init(void);
bool registry_register_user(int user_id, const char* queue_name);
bool registry_unregister_user(int user_id);
int registry_get_active_users(int user_ids[], char queue_names[][QUEUE_NAME_LEN], int max_users, int exclude_user_id);
void registry_cleanup(void);

#endif