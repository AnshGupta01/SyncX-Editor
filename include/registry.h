#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdbool.h>

#define MAX_USERS 5
#define REGISTRY_NAME "/synctext_registry"

typedef struct {
    int user_id;
    char socket_path[108];
    int active;
} UserEntry;

typedef struct {
    UserEntry users[MAX_USERS];
    int count;
} UserRegistry;

bool registry_init(void);
bool registry_register_user(int user_id, const char* socket_path);
bool registry_unregister_user(int user_id);
int registry_get_active_users(char socket_paths[][108], int max_users, int exclude_user_id);
void registry_cleanup(void);

#endif
