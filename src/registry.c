#include "registry.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static UserRegistry* registry = NULL;
static int shm_fd = -1;

bool registry_init(void) {
    shm_fd = shm_open(REGISTRY_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return false;
    }

    if (ftruncate(shm_fd, sizeof(UserRegistry)) < 0) {
        perror("ftruncate");
        close(shm_fd);
        return false;
    }
    
    registry = mmap(NULL, sizeof(UserRegistry), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (registry == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return false;
    }

    return true;
}

bool registry_register_user(int user_id, const char* queue_name) {
    if (!registry) return false;

    // Clean stale entry
    for (int i = 0; i < MAX_USERS; i++) {
        if (registry->users[i].user_id == user_id && registry->users[i].active) {
            registry->users[i].active = 0;
            printf("Cleaned stale entry for user_%d\n", user_id);
        }
    }

    // Register
    for (int i = 0; i < MAX_USERS; i++) {
        if (registry->users[i].active == 0) {
            registry->users[i].user_id = user_id;
            strncpy(registry->users[i].queue_name, queue_name, QUEUE_NAME_LEN - 1);
            registry->users[i].active = 1;
            
            if (i >= registry->count) {
                registry->count = i + 1;
            }
            return true;
        }
    }
    return false;
}

bool registry_unregister_user(int user_id) {
    if (!registry) return false;

    for (int i = 0; i < registry->count; i++) {
        if (registry->users[i].user_id == user_id && registry->users[i].active) {
            registry->users[i].active = 0;
            return true;
        }
    }
    return false;
}

int registry_get_active_users(int user_ids[], char queue_names[][QUEUE_NAME_LEN], int max_users, int exclude_user_id) {
    if (!registry) return 0;

    int count = 0;
    for (int i = 0; i < registry->count && count < max_users; i++) {
        if (registry->users[i].active && registry->users[i].user_id != exclude_user_id) {
            user_ids[count] = registry->users[i].user_id;
            strncpy(queue_names[count], registry->users[i].queue_name, QUEUE_NAME_LEN);
            count++;
        }
    }
    return count;
}

void registry_cleanup(void) {
    if (registry) {
        munmap(registry, sizeof(UserRegistry));
        registry = NULL;
    }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_fd = -1;
    }
}