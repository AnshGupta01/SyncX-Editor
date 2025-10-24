# DESIGN DOCUMENT - SyncX Collaborative Text Editor

**Roll Number:** 25CS60R34  
**Project:** CRDT-Based Collaborative Text Editor  
**Date:** October 2025

---

## 1. System Overview

SyncText is a real-time collaborative text editor that allows multiple users (up to 5) to edit a shared document simultaneously using CRDT-based conflict resolution. Each user maintains a local copy of the document that automatically synchronizes with other users through POSIX message queues.

**Key Features:**
- Real-time collaborative editing
- CRDT Last-Writer-Wins (LWW) merge strategy
- Automatic change detection via file monitoring
- POSIX message queue-based communication
- Shared memory user registry

---

## 2. Architecture

### 2.1 Component Design

The system consists of seven main modules:

1. **Registry** (`registry.c/h`) - Shared memory-based user discovery
2. **File Watcher** (`watcher.c/h`) - Monitors local file changes using `stat()`
3. **CRDT** (`crdt.c/h`) - Conflict-free document merging
4. **Broadcaster** (`broadcaster.c/h`) - Sends operations to peers via message queues
5. **Receiver** (`receiver.c/h`) - Receives and processes remote operations
6. **Operation** (`operation.c/h`) - Operation serialization/deserialization
7. **Utils** (`utils.c/h`) - Terminal UI and display

### 2.2 Data Flow

```
Local Edit → Watcher Thread → Broadcaster → POSIX MQ → Receiver Thread → CRDT Merge → Display Update
```

### 2.3 Thread Architecture

Each user instance runs 3 concurrent threads:
- **Main Thread**: Handles initialization, cleanup, and periodic UI refresh
- **Watcher Thread**: Monitors local file changes every 500ms using `stat()`
- **Receiver Thread**: Polls message queue for incoming operations every 10ms

---

## 3. Implementation Details

### 3.1 User Registry (Shared Memory)

- Uses POSIX shared memory (`shm_open`) for inter-process communication
- Each user registers with a unique user_id and message queue name
- Registry maintains active user list and queue names
- Path: `/dev/shm/synctext_registry`

### 3.2 Change Detection

The watcher thread:
1. Polls file modification time using `stat()` every 500ms
2. Reads file and compares with previous snapshot line-by-line
3. Generates operations (Insert/Replace/Delete) for changed lines
4. Passes operations to broadcaster immediately

### 3.3 Message Queue Communication

- Each user has a dedicated POSIX message queue: `/queue_user_<id>`
- Queue attributes: `mq_maxmsg=10`, `mq_msgsize=sizeof(Operation)`
- Non-blocking mode with retry logic on queue full (`EAGAIN`)
- Operations are serialized as fixed-size structs (~4KB)

### 3.4 CRDT Merge Algorithm

**Last-Writer-Wins (LWW) Strategy:**
- Each operation carries: `timestamp`, `user_id`, `line_no`, `op_type`, `new_text`
- Conflict resolution: newer timestamp wins; ties broken by lower user_id
- Document maintains metadata per line: `(timestamp, user_id)`

**Merge Logic:**
```c
if (op.timestamp > line.timestamp || 
    (op.timestamp == line.timestamp && op.user_id < line.user_id)) {
    apply_operation(op);
}
```

**Operation Types:**
- **Insert (I)**: Add new line
- **Replace (R)**: Modify existing line
- **Delete (D)**: Replace line with newline

### 3.5 Lock-Free Programming

- Atomic operations avoided; single-writer-multiple-reader pattern
- Each thread has dedicated data structures
- POSIX message queues provide natural synchronization
- Receiver thread is the sole writer to CRDT document

---

## 4. Data Structures

### 4.1 Operation Structure
```c
typedef struct {
    int user_id;
    char op_type;           // 'I', 'R', or 'D'
    int line_no;
    int col_start, col_end;
    time_t timestamp;
    char old_text[2048];
    char new_text[2048];
} Operation;
```

### 4.2 User Registry Entry
```c
typedef struct {
    int user_id;
    char queue_name[64];
    int active;
} UserEntry;
```

---

## 5. Challenges and Solutions

### Challenge 1: Message Queue Overflow
**Problem:** Large bulk edits (40+ lines) caused some operations to be dropped when queues filled up.

**Solution:** 
- Implemented retry logic with exponential backoff in broadcaster
- Retry up to 5 times with 20ms delay

### Challenge 2: Bulk Operation Synchronization
**Problem:** Rapid multi-line changes sometimes resulted in incomplete synchronization.

**Solution:**
- Immediate broadcasting of each operation (no batching)
- Receiver processes operations immediately on arrival
- Document shrinking to remove trailing empty lines

### Challenge 3: Cross-Platform Compatibility
**Problem:** POSIX message queue behavior differs between Linux and macOS.

**Solution:**
- Would require a lot of changes to conditionally implement POSIX Queues on Linux and IPC on MacOS.

---

## 6. Known Limitations

1. **Bulk Edit Reliability**: Very large edits (50+ lines) may occasionally miss 1-2 lines due to message queue capacity constraints
2. **Single Machine**: Currently designed for same-machine operation (shared memory)
3. **Line-Based Granularity**: Changes tracked per-line, not character-level
4. **No Persistence**: Document state lost on all users exit

---

## 7. Future Enhancements

- Character-level CRDT for finer granularity
- Network-based communication for distributed deployment
- Undo/redo support with operation history

---

## 8. Build and Run

**Compilation & Cleanup:**
```bash
# Clean first then compile
make clean && make

# Clean overall
make clean
```

**Execution:**
```bash
./sync 1  # Terminal 1
./sync 2  # Terminal 2
./sync 3  # Terminal 3
```

## 9. References
1. CRDT: Conflict-Free Replicated Data Types - Marc Shapiro et al.
2. POSIX Message Queues: `man mq_overview`
3. POSIX Shared Memory: `man shm_overview`

---

**End of Design Document**