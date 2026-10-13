# DESIGN DOCUMENT - SyncX Collaborative Text Editor

**Roll Number:** 25CS60R34  
**Project:** CRDT-Based Collaborative Text Editor  
**Date:** October 2025

---

## 1. System Overview

SyncText is a real-time collaborative text editor that allows multiple users (up to 5) to edit a shared document simultaneously. It uses a Conflict-Free Replicated Data Type (CRDT) approach to ensure eventual consistency without a central server. Each user runs an instance of the editor on the same machine, and changes are propagated via POSIX message queues.

---

## 2. System Architecture

### a) High-level Design Overview

The architecture is decentralized, with each user process running independently. A shared memory segment acts as a registry for user discovery. Each user process has three main threads: one for UI, one for watching file changes, and one for receiving changes from other users. When a local file change is detected, it is converted into an operation and broadcast to all other users' message queues.

**Key Features:**
- Real-time collaborative editing for up to 5 users on a single machine.
- CRDT Last-Writer-Wins (LWW) merge strategy for conflict resolution.
- Automatic change detection via file monitoring (`stat()`).
- POSIX message queues for inter-process communication.
- Shared memory for user registry and discovery.

### b) Major Components and Interaction

The system consists of seven main modules:
1.  **Registry (`registry.c/h`)**: Manages user discovery via a POSIX shared memory segment. Each user registers their process ID and message queue name.
2.  **File Watcher (`watcher.c/h`)**: Monitors the local file for changes by polling its modification time.
3.  **CRDT (`crdt.c/h`)**: Implements the LWW merge logic to apply incoming operations and resolve conflicts.
4.  **Broadcaster (`broadcaster.c/h`)**: Sends locally generated operations to the message queues of all other active users.
5.  **Receiver (`receiver.c/h`)**: Listens on the user's own message queue for incoming operations from peers.
6.  **Operation (`operation.h`)**: Defines the `Operation` struct shipped as raw bytes on POSIX MQ (no separate serializer).
7.  **Utils (`utils.c/h`)**: Provides utility functions for the terminal UI and display.

**Data Flow:**
`Local Edit → Watcher Thread → Broadcaster → POSIX MQ → Receiver Thread → CRDT Merge → Display Update`

### c) Key Data Structures

**Operation Structure:**
```c
typedef struct {
    int user_id;
    char op_type;           // 'I' (Insert), 'R' (Replace), or 'D' (Delete)
    int line_no;
    time_t timestamp;
    char new_text[2048];
} Operation;
```

**User Registry Entry:**
```c
typedef struct {
    int user_id;
    char queue_name[64];
    int active;
} UserEntry;
```

---

## 3. Implementation Details

### a) Thread Architecture and Communication

Each user instance runs three concurrent threads:
-   **Main Thread**: Handles initialization, cleanup, and periodic UI refreshes to display the document.
-   **Watcher Thread**: Polls the local file's modification time using `stat()` every 500ms. On detecting a change, it computes the difference and generates operations.
-   **Receiver Thread**: Polls the user’s POSIX message queue for incoming operations every 10ms and passes them to the CRDT module for merging.

Inter-thread communication is minimized. The CRDT heap is protected by an in-process `pthread_mutex` (`crdt_merge_and_save`); the broadcaster sends immediately on the watcher stack (no in-process op buffer).

### b) Change Detection

The file monitoring approach is implemented in the watcher thread:
1.  It records the file's last modification time.
2.  Every 500ms, it calls `stat()` on the file path to get the current modification time.
3.  If the timestamp has changed, it reads the entire file and compares it line-by-line with an in-memory snapshot of the previous version.
4.  For each changed line, it generates an `Insert`, `Replace`, or `Delete` operation and passes it to the broadcaster.

### c) Message Queues and Shared Memory

-   **Shared Memory**: A single POSIX shared memory segment (`/dev/shm/synctext_registry`) is used for the user registry. This allows processes to discover each other and find the correct message queue names to send operations to.
-   **Message Queues**: Each user creates a dedicated POSIX message queue named `/queue_user_<id>`. The broadcaster sends operations to these queues, and the receiver reads from its own queue. Queues are configured to be non-blocking with a maximum capacity of 10 messages.

### d) CRDT Merge Algorithm

The implementation uses a **Last-Writer-Wins (LWW)** strategy on a per-line basis.
-   Each `Operation` is tagged with a `timestamp` and the `user_id`.
-   The core document data structure maintains a timestamp and user ID for each line.
-   **Conflict Resolution**: When an operation is received for a line, a conflict may occur if the line was modified locally. The conflict is resolved by comparing timestamps. The operation with the newer timestamp is applied. If timestamps are identical, the operation from the user with the lower `user_id` is chosen as the winner. This ensures all sites converge to the same state.

**Merge Logic:**
```c
if (op.timestamp > line.timestamp || 
    (op.timestamp == line.timestamp && op.user_id < line.user_id)) {
    apply_operation(op);
}
```

---

## 4. Design Decisions

### a) Important Implementation Choices and Rationale

-   **CRDT Choice (Last-Writer-Wins)**: LWW was chosen for its simplicity and low overhead. It does not require complex tombstones or vector clocks, making it easy to implement with simple timestamps. Since the project focuses on line-based edits rather than character-level granularity, the risk of unintentional data loss from concurrent edits on the same line was considered an acceptable trade-off for implementation speed.
-   **Polling-Based File Watching**: `stat()` polling was used instead of `inotify` for simplicity and portability. While `inotify` is more efficient, it adds complexity in handling different event types. A 500ms polling interval was chosen as a balance between responsiveness and performance overhead.
-   **Single-Machine IPC**: The decision to use POSIX shared memory and message queues was based on the requirement for a single-machine environment. This avoids the complexity of network programming (sockets, serialization for network transport, etc.).

### b) How You Ensured Lock-Free Operation

The architecture was designed to avoid explicit locks (mutexes, semaphores) by adhering to a single-writer-multiple-reader pattern for critical data.
-   Each thread has its own dedicated data structures where possible.
-   The CRDT document state is only ever modified by the **Receiver Thread**. The Watcher and Main threads read from it but never write to it.
-   POSIX message queues provide natural synchronization for passing operations between processes, serializing access to the shared data.

### c) Trade-offs You Made

1.  **Granularity vs. Simplicity**: The line-based CRDT is much simpler than a character-based one but can be inefficient for small edits and may lead to overwriting valid changes if two users edit the same line concurrently.
2.  **Performance vs. Reliability**: The message queue size is limited (`mq_maxmsg=10`). During large bulk edits, this can cause the queue to fill, leading to dropped operations. A retry mechanism was added to mitigate this, but it’s a trade-off between memory usage and guaranteed delivery.
3.  **Portability vs. Features**: The use of POSIX-specific IPC mechanisms limits the application to compliant systems (like Linux) and makes it non-portable to systems like Windows without significant rework.

---

## 5. Challenges and Solutions

### a) Major Difficulties Faced

-   **Message Queue Overflow**: During testing with large copy-paste operations (40+ lines), the broadcaster would generate operations faster than the receiver could process them, causing the message queue to fill up (`EAGAIN` error) and drop subsequent operations.
-   **Bulk Operation Synchronization**: Rapid, large multi-line changes sometimes resulted in an inconsistent state between users. Because operations were processed individually, a user might see a partially-updated document before all operations for the bulk change had arrived.
-   **Cross-Platform Compatibility**: Initial attempts to compile on macOS failed because POSIX message queue behavior and limits differ significantly from Linux. The project was then scoped to be Linux-only.

### b) How You Debugged and Resolved Issues

-   **Logging**: Extensive logging was added to the broadcaster and receiver to trace the flow of operations. Each operation was printed with its timestamp, user ID, and content as it was sent and received. This helped identify when and where operations were being dropped.
-   **Issue Isolation**: To debug queue overflows, I created a specific test case that involved pasting a 50-line text block. By observing the logs, I confirmed the `EAGAIN` error was the root cause. The solution was to implement a retry loop with an exponential backoff in the broadcaster, giving the receiver time to catch up.
-   **State Dumps**: A debug function was added to print the entire CRDT document state (including line metadata) for each user. When an inconsistency was found, I could compare the state dumps from different users to pinpoint the exact line and operation that caused the divergence.

---

## 6. Future Enhancements

-   Character-level CRDT for finer-grained editing.
-   Network-based communication (TCP/IP) to support distributed collaboration over the internet.
-   Undo/redo support by maintaining a history of operations.

---

## 7. References

1.  CRDT: Conflict-Free Replicated Data Types - Marc Shapiro et al.
2.  POSIX Message Queues: `man mq_overview`
3.  POSIX Shared Memory: `man shm_overview`

---

**End of Design Document**