#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static UserRegistry* registry = NULL;
static int shm_fd = -1;

bool registry_init(void) {
    shm_fd = shm_open(REGISTRY_NAME, O_CREAT | O_RDWR, 0666);
    
    if (shm_fd < 0) {
        perror("[REGISTRY] shm_open");
        return false;
    }
    
    if (ftruncate(shm_fd, sizeof(UserRegistry)) < 0) {
        perror("[REGISTRY] ftruncate");
        close(shm_fd);
        return false;
    }
    
    registry = mmap(NULL, sizeof(UserRegistry), PROT_READ | PROT_WRITE, 
                    MAP_SHARED, shm_fd, 0);
    
    if (registry == MAP_FAILED) {
        perror("[REGISTRY] mmap");
        close(shm_fd);
        return false;
    }
    
    printf("[REGISTRY] Initialized successfully\n");
    return true;
}

bool registry_register_user(int user_id, const char* queue_name) {
    if (!registry || !queue_name) return false;
    
    // Clean any stale entry for this user_id
    for (int i = 0; i < MAX_USERS; i++) {
        if (registry->users[i].user_id == user_id && registry->users[i].active) {
            registry->users[i].active = 0;
            printf("[REGISTRY] Cleaned stale entry for user_%d\n", user_id);
        }
    }
    
    // Register new user
    for (int i = 0; i < MAX_USERS; i++) {
        if (registry->users[i].active == 0) {
            registry->users[i].user_id = user_id;
            strncpy(registry->users[i].queue_name, queue_name, QUEUE_NAME_LEN - 1);
            registry->users[i].queue_name[QUEUE_NAME_LEN - 1] = '\0';  // Ensure null termination
            registry->users[i].active = 1;
            
            if (i >= registry->count) {
                registry->count = i + 1;
            }
            
            printf("[REGISTRY] Registered user_%d at slot %d\n", user_id, i);
            return true;
        }
    }
    
    fprintf(stderr, "[REGISTRY] No space available (max %d users)\n", MAX_USERS);
    return false;
}

bool registry_unregister_user(int user_id) {
    if (!registry) return false;
    
    for (int i = 0; i < registry->count; i++) {
        if (registry->users[i].user_id == user_id && registry->users[i].active) {
            registry->users[i].active = 0;
            printf("[REGISTRY] Unregistered user_%d\n", user_id);
            return true;
        }
    }
    
    return false;
}

int registry_get_active_users(int user_ids[], char queue_names[][QUEUE_NAME_LEN], 
                               int max_users, int exclude_user_id) {
    if (!registry || !user_ids || !queue_names) return 0;
    
    int count = 0;
    for (int i = 0; i < registry->count && count < max_users; i++) {
        if (registry->users[i].active && registry->users[i].user_id != exclude_user_id) {
            user_ids[count] = registry->users[i].user_id;
            strncpy(queue_names[count], registry->users[i].queue_name, QUEUE_NAME_LEN - 1);
            queue_names[count][QUEUE_NAME_LEN - 1] = '\0';  // Ensure null termination
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
    
    printf("[REGISTRY] Cleaned up successfully\n");
}